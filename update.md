# Update Plan: Callback-Oriented Block Audio Pipeline

> Verdict: approve the direction, but not the exact migration plan in `p-block.md`.
> The right move is a clean break: replace the frame-by-frame pipeline outright
> and rebuild around one real-time block render contract.

---

## Goal

Bring audio into the callback thread through a single block-based pull path that:

- does no heap allocation on the callback thread
- does no locking on the callback thread
- does no per-frame virtual dispatch
- keeps audio planar internally
- converts to interleaved only at the device boundary
- is a good base for real multi-channel support later

---

## What To Keep From `p-block.md`

- Block pull from the device callback into the audio graph
- Planar float buffers internally
- Pre-allocated scratch/output buffers
- Interleaved layout only at I/O boundaries
- Block-oriented resampling instead of frame-by-frame resampling

---

## What To Change

The new implementation should **not** keep the following parts of the old plan:

- No dual `read()` / `read_block()` migration layer
- No `AudioFrame` in the core playback pipeline
- No `AudioData` object that is both sample storage and playback cursor
- No per-block "normalize output" step in the mixer
- No `AudioStream` queue of whole `AudioBuffer` objects if producer writes can be arbitrary-sized
- No extra interleave scratch buffer if samples can be written directly into the device buffer

---

## Core Design

### 1. One render contract

Replace the current source interface with a single block render function.

```cpp
enum class RenderState {
    Ok = 0,
    Starved = 1,
    Finished = 2,
    Remove = 3,
};

struct RenderResult {
    uint32_t frames_produced = 0;
    RenderState state = RenderState::Ok;
};

class AudioSource {
public:
    virtual RenderResult render(AudioBlockView dst) = 0;
    virtual ~AudioSource() = default;
};
```

Rules:

- `dst.frame_count` is the exact number requested by the callback this time
- `frames_produced` may be less than `dst.frame_count`
- `state` may be `Finished` or `Remove` even when `frames_produced > 0`
- the callback thread never asks a source to render "up to capacity"; it asks for an exact block

This avoids the biggest correctness hole in the old plan: over-reading when buffer capacity is larger than the current callback size.

### 2. Separate owned buffers from views

Use two types:

- `AudioBuffer`: owns planar storage, pre-allocated, move-only
- `AudioBlockView`: non-owning view over channel pointers for exactly `N` frames

Suggested shape:

```cpp
struct AudioBlockView {
    static constexpr uint32_t MAX_CHANNELS = 8;
    std::array<Sample *, MAX_CHANNELS> channels{};
    uint32_t frame_count = 0;
    uint8_t channel_count = 0;
};

class AudioBuffer {
private:
    std::unique_ptr<Sample[]> storage;
    std::array<Sample *, AudioBlockView::MAX_CHANNELS> channel_ptrs{};
    uint32_t frame_capacity = 0;
    uint8_t channel_count = 0;

public:
    AudioBuffer(uint32_t p_frame_capacity, uint8_t p_channel_count);
    AudioBuffer(AudioBuffer &&other) noexcept;
    AudioBuffer &operator=(AudioBuffer &&other) noexcept;
    AudioBuffer(const AudioBuffer &) = delete;
    AudioBuffer &operator=(const AudioBuffer &) = delete;

    AudioBlockView view(uint32_t p_frame_count);
    void clear(uint32_t p_frame_count);
};
```

`AudioBuffer` should be non-copyable and must use a custom move operation. A default move would transfer
`storage` but leave `channel_ptrs` pointing at the old allocation. The move constructor and move assignment
must recompute channel pointers from the new `storage.get()`.

```cpp
AudioBuffer::AudioBuffer(AudioBuffer &&other) noexcept
    : storage(std::move(other.storage)),
      frame_capacity(other.frame_capacity),
      channel_count(other.channel_count) {
    for (uint8_t ch = 0; ch < channel_count; ch++) {
        channel_ptrs[ch] = storage.get() + ch * frame_capacity;
    }
    other.channel_ptrs = {};
    other.frame_capacity = 0;
    other.channel_count = 0;
}
```

`operator=(AudioBuffer &&)` must perform the same pointer fix-up after taking ownership of `storage`.

### 3. Internal graph format

The graph should run in:

- `float32`
- planar layout
- one graph sample rate
- one graph channel count

All clips and stream inputs should be converted into that graph format before they are mixed.

---

## Data Model

### 1. `AudioClip` instead of cursor-owning `AudioData`

Current `AudioData` mixes two jobs:

- immutable PCM storage
- mutable playback state

That is the wrong split. Replace it with:

- `AudioClip`: immutable planar PCM plus metadata
- `AudioVoice`: playback cursor, gain, pan, loop state, and reference to an `AudioClip`

Why:

- the same sound can overlap correctly
- playback state lives where playback actually happens
- clip storage becomes memcpy-friendly
- callback-side work becomes simpler and more predictable

Suggested direction:

```cpp
class AudioClip {
public:
    SampleRate sample_rate;
    uint8_t channel_count;
    uint64_t frame_count;
    std::unique_ptr<Sample[]> storage;
};

class AudioVoice : public AudioSource {
private:
    const AudioClip *clip = nullptr;
    uint64_t frame_position = 0;
    std::atomic<Volume> volume{};
    std::atomic<Panning> panning{};
};
```

If you want to minimize naming churn, `AudioData` can be kept as the type name, but its semantics should become "immutable clip", not "playable source".

Clip ownership should stay off the callback thread. The simplest rule is:

- `AudioSpace` or another registry owns all `AudioClip` storage
- `AudioVoice` holds a raw `const AudioClip *`
- clips outlive all voices that reference them

Do not rely on `shared_ptr<const AudioClip>` in the render path. If a removed voice holds the final reference,
the clip's storage can be freed on the callback thread, which violates the real-time contract.

That lifetime rule also constrains teardown order:

- stop and drain voices first
- then destroy clip storage on the control thread

### 2. `AudioStreamSource` for live producer input

Do not model streams as a queue of whole blocks unless the public API is also fixed-block.

Use a planar SPSC frame ring instead:

- capacity is measured in frames
- producer can write any frame count
- consumer can read any frame count
- wrap-around is handled by split copy

Suggested APIs:

```cpp
size_t write_interleaved(const Sample *src, size_t frames);
size_t write_planar(AudioBlockView src);
RenderResult render(AudioBlockView dst) override;
```

This is a better fit than a ring of `AudioBuffer` slots because it naturally handles:

- partial producer writes
- callback sizes that vary by backend
- reads/writes that do not line up with the chosen block size

This design is also more subtle than a block ring because wrap-around reads and writes become split copies
per channel. That is acceptable, but wrap-around tests should be added as soon as the ring exists.

### 3. `AudioSpace` becomes an asset/voice facade

`AudioSpace` should stop mixing `AudioData` objects directly.

Instead:

- store `AudioClip` objects by id
- `play(id)` creates a new `AudioVoice`
- `stop(id)` stops voices derived from that clip or a specific voice handle
- the mixer owns active voices, not the asset database

This is the correct shape if the library is meant to play the same sound effect more than once at the same time.

---

## Callback Thread Rules

The callback thread must never:

- allocate
- resize or erase vectors
- take a lock
- perform file I/O
- decode compressed data
- change sample rates
- create or destroy graph objects

The callback thread may:

- pull commands from a lock-free queue
- read/write pre-allocated buffers
- update cursor integers and atomics
- mix blocks
- convert planar float output into the device buffer

Important consequence: the current mixer pattern of `vector.erase()` inside render must go away.

---

## Mixer Design

### 1. Fixed-capacity active source slots

The mixer should own a pre-sized slot array or reserved vector of active renderable sources.

Control thread changes should arrive through a command queue:

- `AddVoice`
- `RemoveVoice`
- `SetVoiceVolume`
- `SetVoicePan`
- `StopAll`

Applying commands in the callback must not allocate. That means:

- no `vector.erase()`
- no `push_back()` that can grow
- no callback-side ownership churn if it can be avoided

Preferred shape:

- preallocate voice slots
- recycle slots with a free list
- remove by marking slot inactive and reusing later

Sketch the slot API before coding the mixer or voice type in isolation. The two pieces are coupled.

Example shape:

```cpp
struct VoiceSlot {
    bool active = false;
    AudioVoice voice;
};

VoiceId add_voice(const AudioClip *clip, VoiceParams params);
void stop_voice(VoiceId id);
void for_each_active_voice(FunctionRef<void(VoiceSlot &)> fn);
```

### 2. Mixing flow

For each callback:

1. clear output block to zero
2. drain pending commands
3. for each active source:
   - render into one scratch block
   - sum scratch into output
   - remove source if result says `Remove`
4. apply master gain
5. write to the device buffer

Do **not** keep the current `normalize_output` behavior.

Reasons:

- per-frame normalization is already a weak design
- block peak normalization changes loudness per callback and will pump audibly
- proper limiting belongs in an explicit DSP stage later, not hidden in the mixer

For now:

- mix in float
- let user-set gain manage headroom
- clamp only at final device conversion

---

## Device Path

### 1. One shared backend write path

Both CoreAudio and WASAPI should go through the same base helper.

Current state:

- CoreAudio already uses `AudioDevice::write_frames()`
- WASAPI still renders frame-by-frame in its own callback loop

Target:

```cpp
void AudioDevice::render_to_device_buffer(
    void *dst,
    uint32_t frame_count,
    uint32_t bytes_per_frame
);
```

### 2. Pre-allocate one mix buffer

At device start:

- query the backend's maximum callback frame count
- allocate one planar `AudioBuffer` with that capacity

At callback time:

1. create a view for `frame_count`
2. clear that view
3. call root source `render()`
4. interleave and convert directly into `dst`
5. zero-fill any remainder if short

Do not add an intermediate interleaved temp buffer unless a backend specifically requires one.

The direct write loop is enough:

```cpp
for (uint32_t frame = 0; frame < produced; frame++) {
    for (uint8_t ch = 0; ch < channel_count; ch++) {
        Sample s = output.channels[ch][frame];
        write_sample_to_device(dst, s);
    }
}
```

This keeps one fewer copy on the hot path.

---

## Reader And Storage Rewrite

### 1. Decode directly to clip storage

Readers should stop returning `std::vector<AudioFrame>`.

Instead:

- decode to a temporary interleaved sample buffer if the codec forces it
- immediately deinterleave once into `AudioClip` planar storage
- keep the clip planar forever after that

This removes a large amount of frame-by-frame conversion work from playback.

### 2. Resample offline when possible

The graph should ideally run at one sample rate.

So:

- file-backed clips should be resampled once when loaded into the space
- callback-time sample-rate conversion should be reserved for live stream sources that truly need it

The current `ReSamplerSource` is unused and frame-based. It should either:

- be deleted, or
- be rebuilt as a block-based planar source wrapper for live input only

---

## Panning And Channel Policy

The buffer design can support up to 8 channels, but the DSP policy still needs to be explicit.

Recommended implementation order:

1. implement mono and stereo correctly
2. make wider channel counts pass through honestly where possible
3. reject channel/panner combinations that are not implemented yet
4. add true multi-channel panning later as a separate task

Do not repeat the current behavior where the type system advertises wider layouts but the frame pipeline silently drops them.

---

## Concrete Implementation Order

### Phase 1: Foundation

Create:

- `src/audio/lowl_audio_buffer.h`
- `src/audio/lowl_audio_buffer.cpp`

Replace or delete:

- delete `src/audio/lowl_audio_block.h`
- stop using `src/audio/lowl_audio_frame.h` in the playback path

Change:

- `src/audio/source/lowl_audio_source.h`
- `src/audio/source/lowl_audio_source.cpp`

Deliverable:

- one render interface
- one owned buffer type
- one view type

### Phase 2: Device callback path

Change:

- `src/audio/backend/lowl_audio_device.h`
- `src/audio/backend/lowl_audio_device.cpp`
- `src/audio/backend/coreaudio/lowl_audio_core_audio_device.cpp`
- `src/audio/backend/wasapi/lowl_audio_wasapi_device.cpp`

Deliverable:

- both backends call the same block render helper
- no per-frame source pull in backend code

### Phase 3: Voice and mixer skeleton

Preferred:

- rename `AudioData` to `AudioClip`
- add `AudioVoice`
- define the mixer's slot model and control-thread command path at the same time

Files:

- `src/audio/source/lowl_audio_data.h/.cpp` or replacement `lowl_audio_clip.*`
- new `src/audio/source/lowl_audio_voice.h/.cpp`
- `src/audio/source/lowl_audio_mixer.h`
- `src/audio/source/lowl_audio_mixer.cpp`

Deliverable:

- immutable clip storage
- independent playback instances
- fixed-capacity active slots
- slot add/remove/update API

### Phase 4: Mixer render path

Change:

- `src/audio/source/lowl_audio_mixer.h`
- `src/audio/source/lowl_audio_mixer.cpp`

Deliverable:

- block mixing with one scratch buffer
- no callback allocations
- remove hidden normalization

### Phase 5: AudioSpace rewrite

Change:

- `src/audio/source/lowl_audio_space.h`
- `src/audio/source/lowl_audio_space.cpp`

Deliverable:

- clip registry
- play spawns voices
- stop/reset/seek target voices or clips explicitly

### Phase 6: Stream rewrite

Change:

- `src/audio/source/lowl_audio_stream.h`
- `src/audio/source/lowl_audio_stream.cpp`
- `src/audio/lowl_audio_utilities.h`
- `src/audio/lowl_audio_utilities.cpp`

Deliverable:

- planar SPSC frame ring
- arbitrary-sized writes and reads
- no `ReaderWriterQueue<AudioFrame>`

### Phase 7: Reader and resampler rewrite

Change:

- `src/audio/reader/*`
- `src/audio/convert/lowl_audio_re_sampler.h`
- `src/audio/convert/lowl_audio_re_sampler_r8b.h`
- `src/audio/convert/lowl_audio_re_sampler_r8b.cpp`

Delete or replace:

- `src/audio/source/lowl_audio_re_sampler_source.h`
- `src/audio/source/lowl_audio_re_sampler_source.cpp`

Deliverable:

- readers produce planar clip data
- offline clip resampling is block/array-based
- runtime resampler exists only if truly needed

### Phase 8: Cleanup

- remove old frame-based source code
- remove obsolete tests that depend on `read(AudioFrame &)`
- update public includes and docs

---

## Test Plan

Add or rewrite tests for:

- exact short-read zero-fill behavior in device output
- `AudioVoice` render end-of-clip semantics
- overlapping playback of the same clip
- mixer accumulation across multiple active voices
- `AudioStreamSource` partial write, partial read, and wrap-around cases
- `AudioBuffer` move constructor and move assignment pointer fix-up
- reader decode into planar storage
- resampler frame-count correctness
- callback render using varying requested frame counts

Important regression tests:

- callback requesting 256 frames from a 512-frame-capacity buffer must only advance sources by 256
- a source that finishes mid-block must return `frames_produced > 0` and terminal state in the same call
- both CoreAudio and WASAPI must exercise the shared `render_to_device_buffer()` path

---

## Recommended First Cut

If the goal is to ship the new architecture quickly without dragging old behavior forward, do this first:

1. land foundation types and the new `render()` contract
2. move CoreAudio and WASAPI to one shared block render path
3. rewrite clip storage as planar immutable data
4. implement voice plus mixer slot management together
5. land the mixer block render path
6. rewrite stream as a planar frame ring
7. delete frame-based code

That gets the hot path right first. Reader cleanup and deeper multi-channel work can follow once the callback model is stable.

---

## Bottom Line

Yes, the block-based direction is the right one.

The version worth implementing is:

- clean break, not dual API
- planar float graph
- exact-size block pull from the callback
- immutable clip storage plus playback voices
- fixed-allocation mixer
- planar stream ring for live input
- direct device write from planar output

That is the best path for bringing audio data into the callback thread cleanly and without carrying the current architecture's frame-based limitations forward.
