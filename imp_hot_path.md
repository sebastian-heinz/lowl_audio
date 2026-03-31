# Concrete Fast Render Solution

## Decision

If we remove compatibility constraints and optimize only for:

- fastest possible render path
- standalone composable components
- independent control from the outside
- simple public usage

then the right solution is not to keep patching the current `AudioSource::render()` + scratch-buffer design.

The right solution is to replace it with a fixed internal mix engine built around:

- `float32` planar aligned internal buffers
- direct `mix_into()` rendering, never `render()` into scratch on the common path
- buses that compose gain and forward work downward instead of always allocating their own buffer
- preprocessed clip assets at engine sample rate and engine channel layout
- stream sources that already match engine layout/rate
- runtime-dispatched SIMD kernels for the hot cases
- backend paths that prefer native `float32`, and use non-interleaved `float32` where the OS allows it

That is the concrete solution.

## End-State Architecture

### 1. Fixed Internal Render Domain

The engine should mix in exactly one internal format:

- sample type: `float`
- layout: planar
- alignment: 64-byte aligned per channel
- channel stride: padded to a SIMD-friendly multiple

Do not allow the hot path to do any of these dynamically:

- sample format conversion
- resampling
- channel-layout conversion

Those happen either:

- at asset load time for immutable clips
- on the producer side for streams
- or in explicit adapter nodes outside the hot mix kernel

This gives one stable, vectorizable render domain.

### 2. Replace `AudioSource::render()` With `AudioNode::mix_into()`

The core interface should become:

```cpp
struct GainVector {
    float gain[8];
};

struct MixContext {
    uint32_t frames;
    double sample_rate;
};

struct MixResult {
    uint32_t frames_produced;
    bool active;
    bool finished;
    bool error;
};

class AudioNode {
public:
    virtual ~AudioNode() = default;
    virtual MixResult mix_into(const MixContext &ctx,
                               const GainVector &upstream_gain,
                               AudioBlockView dst) = 0;
};
```

The important change is this:

- nodes do not render into a temporary block and ask someone else to mix later
- nodes mix directly into the destination accumulator

That is the center of the redesign.

### 3. The Engine Is A Bus Tree

The composable units should be:

- `ClipVoice`
- `StreamVoice`
- `Bus`
- `AudioSpace` as the high-level facade

The tree should look like:

```text
AudioSpace
  -> MasterBus
       -> Bus / ClipVoice / StreamVoice
            -> nested Bus / ClipVoice / StreamVoice
```

### 4. Buses Must Be Cheap

This is the most important structural optimization.

A bus without effects must **not** allocate a local buffer.

Instead:

- the bus computes `combined_gain = upstream_gain * bus_gain`
- then tells each child to mix directly into the parent destination with that combined gain

So a pure grouping bus is just:

```cpp
for (child : children) {
    child->mix_into(ctx, combined_gain, dst);
}
```

That means nested composition is basically free.

A bus only gets a local accumulation buffer when it actually needs one:

- effect chain
- recording tap
- metering tap that needs isolated local signal
- send/return routing

So the rule is:

- no FX: forward directly into parent accumulator
- with FX: allocate one local buffer, mix children there once, process once, then accumulate once into parent

This preserves composition and avoids pointless memory passes.

## Concrete Node Types

### `ClipVoice`

`ClipVoice` owns:

- shared `AudioData`
- render-thread transport state
- control snapshot state
- cached channel gains

It reads directly from `AudioData` channel pointers and accumulates into `dst`.

It does not:

- copy into scratch
- run separate volume and panning passes
- call generic per-sample conversion code

Hot loop:

```cpp
dst_L[i] += src_L[i] * gain_L;
dst_R[i] += src_R[i] * gain_R;
```

That is the common stereo path.

### `StreamVoice`

`StreamVoice` owns:

- planar ring buffer
- render-thread read cursor
- control snapshot state
- cached channel gains

It mixes directly from the ring buffer into `dst`.

Because the ring is planar, it needs at most two SIMD calls per channel:

- first contiguous segment
- second wrapped segment

It never copies to scratch first.

### `Bus`

`Bus` owns:

- active child list
- its own gain/pan control state
- optional effect chain

The bus itself is not the hot arithmetic bottleneck if it avoids local buffering in the no-FX case.

### `AudioSpace`

`AudioSpace` becomes only a facade:

- asset registry
- voice/stream/bus handle lookup
- control-thread command submission
- user-friendly creation APIs

It should not be doing per-callback state pushes like:

```cpp
mixer->set_volume(get_volume());
mixer->set_panning(get_panning());
```

The render tree already has the gain state it needs.

## Control Model

Each node should have two state domains.

### Control State

Written from non-audio threads:

- play/pause/stop
- seek
- volume
- pan
- looping
- bus attach/detach

### Render State

Owned by the audio thread:

- current frame position
- cached gain vector
- cached last-seen generation
- stream read cursor

The control thread should not directly update hot render arrays.

Use:

- atomics for small scalar controls
- lock-free command queue for structural edits
- generation counter to tell the render thread when cached gains must be recomputed

That gives independent control without polluting the hot loops.

## Gain Model

### Use A Cached Per-Channel Gain Vector

Do not keep separate runtime passes for:

- voice volume
- voice pan
- bus volume
- master volume

Every node exposes one cached per-channel gain vector.

For a node mixing into a parent bus:

```text
effective_gain[channel] = upstream_gain[channel] * local_gain[channel]
```

This should be computed outside the frame loop.

### Stereo Pan

For stereo, use equal-power pan:

```cpp
left  = volume * sqrt(1.0f - pan);
right = volume * sqrt(1.0f + pan);
```

But compute it only when control generation changes.

### Surround / Generic Layouts

If surround must remain supported, keep:

- `GainVector` for same-layout buses
- `GainMatrix` only for explicit format/layout adapter nodes

Do not pay matrix-mix cost on the normal same-layout path.

## Asset And Stream Rules

### Immutable Clips

`AudioData` should be preprocessed at load/attach time to the engine's fixed internal format:

- `float32`
- engine sample rate
- engine channel layout
- planar aligned storage

That means `ClipVoice` never resamples or remaps channels during the callback.

### Streams

`StreamVoice` should be created in engine format:

- engine sample rate
- engine channel layout
- planar ring storage

If external data arrives in another format, conversion must happen:

- on the producer thread
- or through an explicit adapter node outside the hot mix kernel

This is a deliberate design choice. It keeps the hot path clean.

## Backend Strategy

### Prefer `FLOAT_32` Always

When opening a device, the engine should try formats in this order:

1. `FLOAT_32`
2. `INT_16`
3. other formats only as fallback

The hot path should be built around the assumption that float output is the normal case.

### CoreAudio: Use Non-Interleaved Float32

This is the biggest backend-specific win available in the current target set.

CoreAudio can be configured for non-interleaved `float32`. If we do that:

- there is no planar-to-interleaved conversion
- there is no float conversion
- the master bus can render directly into the AudioUnit output buffers

So on CoreAudio the best path becomes:

```text
clear device buffers
master_bus.mix_into(device_planar_buffers)
return
```

That is better than "mix then interleave".

The CoreAudio backend should be changed to request:

- `FLOAT_32`
- non-interleaved PCM

and to render one channel per `ioData->mBuffers[channel]`.

### WASAPI: Use Float32 Interleaved

WASAPI will generally remain interleaved.

So on WASAPI:

- mix into internal planar float32
- run one final interleave kernel

If WASAPI forces integer output:

- run one final typed conversion kernel

### Float Output Should Not Clamp

For float device targets:

- do not clamp to `[-1, 1]`

Clamping float output is wasted arithmetic and changes behavior unnecessarily.

For integer outputs:

- clamp in the converter
- nowhere else

## SIMD Strategy

### Do Not Hope The Compiler Solves Everything

The loops should still be written in an autovectorization-friendly way, but the final design should include explicit SIMD kernels for the common cases.

Use runtime dispatch selected once at startup.

### Required Kernels

At minimum:

- `mix_mono_to_mono_f32`
- `mix_mono_to_stereo_f32`
- `mix_stereo_to_stereo_f32`
- `accumulate_planar_scaled_f32`
- `interleave_stereo_f32`
- `convert_interleave_stereo_i16`

### ISA Targets

- `arm64`: NEON/FMA path
- `x86_64`: AVX2/FMA path
- fallback: scalar portable path

### Dispatch Model

At startup:

- detect CPU features
- fill a small dispatch table

For example:

```cpp
struct MixKernels {
    void (*mix_stereo_to_stereo)(const float *src_l, const float *src_r,
                                 float *dst_l, float *dst_r,
                                 float gain_l, float gain_r,
                                 uint32_t frames);
    void (*interleave_f32_stereo)(const float *src_l, const float *src_r,
                                  float *dst, uint32_t frames);
};
```

The callback must not be doing feature checks.

### Denormals

Set flush-to-zero / denormals-are-zero on the audio thread:

- x86: FTZ + DAZ in `MXCSR`
- ARM: flush-to-zero in FP control state if available

This matters for quiet tails and reverb-like future effects.

## Memory Layout

### Aligned Buffers

Replace `std::make_unique<Sample[]>` channel storage for hot buffers with explicit aligned allocation.

Use:

- 64-byte alignment
- padded channel stride

Apply this to:

- `AudioData`
- mix buffers
- stream ring buffers

### Dense Active Lists

Each bus should maintain a dense active child list for the render thread.

Do not scan sparse arrays on every callback if avoidable.

If handles are needed for outside control, keep handles in the control layer and keep the render list dense.

## Concrete Public API Shape

The user-facing API should stay simple even though the engine is being rebuilt.

Recommended outside-facing objects:

- `AudioSpace`
- `AudioBusHandle`
- `AudioVoiceHandle`
- `AudioStreamHandle`
- `AudioAssetHandle`

Recommended usage:

```cpp
AudioSpace space(48000.0, ChannelLayout::Stereo);

auto music_bus = space.create_bus(space.master_bus());
auto sfx_bus = space.create_bus(space.master_bus());

auto clip = space.add_audio("music.wav");
auto voice = space.play_clip(clip, music_bus);

space.set_volume(voice, 0.7f);
space.set_panning(voice, -0.2f);
space.set_volume(music_bus, 0.8f);
```

Internally this becomes a bus tree with direct `mix_into()` rendering.

That satisfies the requirement that components stay standalone and composable.

## Exact Structural Changes To Make

### Delete From The Hot Path

These should disappear from the render path entirely:

- `AudioSource::process_volume()`
- `AudioSource::process_panning()`
- per-source scratch-buffer rendering for voices/streams
- mixer post-pass master gain/pan
- per-sample `switch(format)` inside the callback
- float-output clamping

### Replace The Base Types

Replace `AudioSource` with a leaner `AudioNode` interface:

- `mix_into(...)`
- no generic "render then someone else mixes"

`AudioMixer` should become `Bus`.

`AudioSpace` should manage a bus tree, not just a flat mixer plus voices.

### Rebuild `AudioData`

Store:

- aligned planar `float32`
- preconverted to engine layout/rate

### Rebuild `AudioStream`

Store:

- aligned planar ring buffer
- engine layout/rate only

### Rebuild Backend Output

CoreAudio:

- request non-interleaved `float32`
- render directly into device channel buffers

WASAPI:

- request `float32` where possible
- otherwise do one typed final conversion

## CMake Changes

The build system should explicitly support the SIMD split.

### Global Non-Debug Optimization

Use at least:

- Clang/GCC: `-O3`
- MSVC: `/O2`

### Kernel Translation Units

Compile SIMD files separately.

Suggested layout:

- `src/audio/simd/lowl_mix_scalar.cpp`
- `src/audio/simd/lowl_mix_neon.cpp`
- `src/audio/simd/lowl_mix_avx2.cpp`
- `src/audio/simd/lowl_convert_scalar.cpp`
- `src/audio/simd/lowl_convert_neon.cpp`
- `src/audio/simd/lowl_convert_avx2.cpp`

Suggested flags:

- scalar: normal project flags
- AVX2 TU: `-mavx2 -mfma` or MSVC equivalent
- NEON TU: default on `arm64`, plus `-ffp-contract=fast`

Use `-ffp-contract=fast` on kernel TUs.

If maximum speed is more important than IEEE corner cases, enable:

- `-ffast-math`
- `/fp:fast`

only on the dedicated kernel translation units, not globally.

### Runtime Dispatch

Do not use `-march=native` as the main strategy.

This is a library. Use runtime dispatch instead.

## Concrete Render Path

### CoreAudio Float32 Non-Interleaved

Best-case render path:

```text
callback
  -> apply queued control edits
  -> clear device output buffers
  -> master_bus.mix_into(unity_gain, device_planar_buffers)
  -> done
```

That is the best achievable path in this project.

### WASAPI Float32 Interleaved

```text
callback
  -> apply queued control edits
  -> clear internal planar master buffer
  -> master_bus.mix_into(unity_gain, master_buffer)
  -> interleave_stereo_f32(master_buffer, device_buffer)
  -> done
```

### WASAPI Int16

```text
callback
  -> apply queued control edits
  -> clear internal planar master buffer
  -> master_bus.mix_into(unity_gain, master_buffer)
  -> convert_interleave_stereo_i16(master_buffer, device_buffer)
  -> done
```

## Why This Is Better Than The Earlier Proposal

The earlier proposal was correct about the arithmetic and memory goal, but it was still framed as:

- voice renders raw data
- mixer knows about voice-style access
- then optimize conversion

The better solution is broader and cleaner:

- every hot source type mixes directly
- buses compose gain without forcing extra buffers
- backend chooses the cheapest device target possible
- only effect buses pay for local accumulation

That is both faster and more composable.

## Bottom Line

If the goal is the best render path rather than incremental compatibility, then the engine should be rebuilt around:

- fixed aligned planar `float32` internals
- direct `mix_into()` nodes
- gain composition through bus trees
- preprocessed clip assets
- stream sources that already match engine format
- explicit SIMD kernels with runtime dispatch
- CoreAudio non-interleaved `float32` direct render
- WASAPI `float32` first, typed conversion only when forced

That design gives the shortest real hot path while still keeping the system easy to use:

- voices and streams are standalone nodes
- buses provide composition and independent control
- `AudioSpace` remains the simple facade
- the callback becomes almost entirely sequential reads plus FMA-style accumulation
