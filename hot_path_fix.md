# Hot Path Fix: Optimal Render Loop

## Current state: what happens to one sample

```
Load  (AudioData → register)          pass 1: voice copy
Store (register → scratch buffer)
Load  (scratch buffer → register)     pass 2: voice volume
Mul   (register × volume)
Store (register → scratch buffer)
Load  (scratch buffer → register)     pass 3: voice pan
Mul   (register × pan_gain)
Store (register → scratch buffer)
Load  (scratch buffer → register)     pass 4: mixer accumulate
Add   (register + output)
Store (register → output buffer)
Load  (output buffer → register)      pass 5: mixer volume
Mul   (register × master_volume)
Store (register → output buffer)
Load  (output buffer → register)      pass 6: mixer pan
Mul   (register × master_pan_gain)
Store (register → output buffer)
Load  (output buffer → register)      pass 7: clamp + convert + interleave
Clamp
Convert
Store (register → device buffer)
```

7 loads, 7 stores, 3 multiplies, 1 add per sample. Data evicts from L1 and gets reloaded between passes.

## Target state: what should happen

```
Load  (AudioData → register)
FMA   (register × voice_channel_gain + output)
Store (register → output buffer)
... after all sources mixed ...
Load  (output buffer → register)
Convert
Store (register → device buffer)
```

2 loads, 2 stores, 1 FMA, 1 convert per sample. Data touches memory twice total.

---

## Phase 1: Fused gain-scaled accumulate

The biggest win. Collapses passes 1-5 (voice copy, voice gain, voice pan, mixer accumulate) into one FMA loop per source.

### Current flow

```
Voice::render():
    copy_n(audio_data + position, frames, scratch)    // pass 1
    process_volume(scratch)                            // pass 2: multiply every sample
    process_panning(scratch)                           // pass 3: sqrt + multiply every sample

Mixer::render_mixed_block():
    output[f] += scratch[f]                            // pass 4: plain add
    process_volume(output)                             // pass 5
    process_panning(output)                            // pass 6
```

### New flow

Precompute per-channel gains once when volume or panning changes (not every callback):

```cpp
// On set_volume() or set_panning():
channel_gain[L] = volume * sqrt(1.0 - pan)
channel_gain[R] = volume * sqrt(1.0 + pan)
```

The mixer reads directly from the voice's source data and does one FMA pass:

```cpp
const float gain_L = voice->channel_gain(0);
const float gain_R = voice->channel_gain(1);
const float *src_L = voice->audio_data->get_channel_data(0) + pos;
const float *src_R = voice->audio_data->get_channel_data(1) + pos;
float *dst_L = output.channel(0);
float *dst_R = output.channel(1);

for (uint32_t f = 0; f < frames; f++) {
    dst_L[f] += src_L[f] * gain_L;  // one FMA instruction: vfmadd231ps (x86) / fmla (ARM)
    dst_R[f] += src_R[f] * gain_R;  // one FMA instruction
}
```

This is the pattern SIMD was literally designed for. The compiler will vectorize this at `-O2` without any templates or intrinsics. It processes 4-8 frames per iteration depending on vector width.

### What changes

- `AudioVoice`: cache `channel_gain[]` array, recompute only when volume/panning changes via a dirty flag or generation counter. Expose gains and raw audio data to the mixer.
- `AudioVoice::render()`: write raw samples without gain/pan processing when the mixer will handle it.
- `AudioMixer::render_mixed_block()`: perform gain-scaled accumulate instead of plain `+=`.
- `AudioSource::process_volume()` / `process_panning()`: no longer called per-source in the mix path.

### Files

- `src/audio/source/lowl_audio_voice.h/.cpp`
- `src/audio/source/lowl_audio_source.h/.cpp`
- `src/audio/source/lowl_audio_mixer.cpp`

---

## Phase 2: Master gain skip

Currently `process_volume()` and `process_panning()` run on the mixed output every callback even when master gain is 1.0 and pan is center.

### New flow

```cpp
if (master_gain_L != 1.0f || master_gain_R != 1.0f) {
    for (uint32_t f = 0; f < frames; f++) {
        dst_L[f] *= master_gain_L;
        dst_R[f] *= master_gain_R;
    }
}
```

When master gain is 1.0 and pan is center (the common case in AudioSpace), this is zero work. The precomputed `master_channel_gain[]` uses the same cached gain approach as phase 1.

### Files

- `src/audio/source/lowl_audio_mixer.cpp`
- `src/audio/source/lowl_audio_source.h/.cpp`

---

## Phase 3: Template interleave + convert

Replaces the per-sample `switch(format)` + `clamp` in `render_to_device_buffer()` with a switch-dispatched template. One predictable branch per callback, then a tight typed loop the compiler can see and vectorize entirely.

### Why template, not function pointer

A function pointer is opaque — the compiler cannot inline through it, so it cannot vectorize the loop body. A switch calling a template function is a direct call. After the branch, the compiler has full visibility into the typed loop.

### Why stereo specialization inside the template

The generic template has `block.channel_count` as a runtime variable. The compiler cannot determine the interleave stride and will generate conservative scalar code. With a stereo fast path, the channel count is a compile-time constant and the compiler can pick the optimal shuffle/pack instructions.

### Structure

Type traits map `SampleFormat` to native type and conversion:

```cpp
template <SampleFormat F> struct FormatTraits;

template <> struct FormatTraits<SampleFormat::FLOAT_32> {
    using Type = float;
    static Type convert(Sample s) { return s; }
};

template <> struct FormatTraits<SampleFormat::INT_16> {
    using Type = int16_t;
    static Type convert(Sample s) {
        return static_cast<int16_t>(std::clamp(s, -1.0f, 1.0f) * 32767.0f);
    }
};

// One per format. Clamp is folded into conversion — no separate clamp pass.
```

Each template has a stereo fast path:

```cpp
template <SampleFormat F>
void interleave_convert(const AudioBlockView &block, void *dst, uint32_t frames) {
    using T = typename FormatTraits<F>::Type;
    T *out = static_cast<T *>(dst);

    if (block.channel_count == 2) {
        const Sample *L = block.channel(0);
        const Sample *R = block.channel(1);
        for (uint32_t f = 0; f < frames; f++) {
            out[f * 2]     = FormatTraits<F>::convert(L[f]);
            out[f * 2 + 1] = FormatTraits<F>::convert(R[f]);
        }
        return;
    }

    // Generic N-channel fallback
    const uint8_t ch = block.channel_count;
    for (uint32_t f = 0; f < frames; f++) {
        for (uint8_t c = 0; c < ch; c++) {
            out[f * ch + c] = FormatTraits<F>::convert(block.channel(c)[f]);
        }
    }
}
```

Dispatched once per callback in `render_to_device_buffer()`:

```cpp
switch (published_properties.sample_format) {
    case SampleFormat::FLOAT_32: interleave_convert<SampleFormat::FLOAT_32>(block, dst, frames); break;
    case SampleFormat::INT_16:   interleave_convert<SampleFormat::INT_16>(block, dst, frames);   break;
    case SampleFormat::INT_32:   interleave_convert<SampleFormat::INT_32>(block, dst, frames);   break;
    case SampleFormat::INT_24:   interleave_convert<SampleFormat::INT_24>(block, dst, frames);   break;
    case SampleFormat::FLOAT_64: interleave_convert<SampleFormat::FLOAT_64>(block, dst, frames); break;
    case SampleFormat::INT_8:    interleave_convert<SampleFormat::INT_8>(block, dst, frames);    break;
    case SampleFormat::U_INT_8:  interleave_convert<SampleFormat::U_INT_8>(block, dst, frames);  break;
    default: std::memset(dst, 0, total_bytes); break;
}
```

### What the compiler generates for stereo FLOAT_32

The stereo FLOAT_32 path is a load-pair + interleaved-store loop. Clang/GCC emit `unpcklps`/`unpckhps` on x86 or `vzip`/`st2` on ARM NEON, processing 4 frames per iteration.

### What the compiler generates for stereo INT_16

The INT_16 path is: float multiply → clamp → convert to int32 → pack to int16 → interleaved store. Clang emits `vcvtps2dq` + `vpackssdw` on x86, processing 4-8 frames per iteration.

### Files

- `src/audio/backend/lowl_audio_device.cpp` — new switch + template dispatch replaces inner loop
- `src/audio/convert/lowl_audio_sample_converter.h` — `FormatTraits` and `interleave_convert` templates

---

## Why each technique matters where it does

| Stage | Technique | Why this technique here |
|-------|-----------|----------------------|
| Mix (phase 1) | FMA loop, precomputed gains | Arithmetic reduction (3 muls → 1 FMA), memory reduction (4 passes → 1). Plain loops — no templates needed, the compiler vectorizes FMA patterns automatically. |
| Master gain (phase 2) | Skip when unity | Zero work in the common case. Branches are fine here — one branch per callback, always predicted. |
| Convert (phase 3) | Template + stereo specialization | Compiler needs known types and strides at compile time to emit SIMD shuffle/pack instructions. This is the one place where templates earn their complexity. |

## Implementation order

Phase 3 is self-contained — it only touches `render_to_device_buffer()` and the sample converter header. No architectural changes. Can be done and tested independently.

Phase 2 is trivial once gain caching exists.

Phase 1 is the biggest change. It alters the contract between voices and the mixer (voices expose raw data + gains, mixer does the fused accumulate). It should be done carefully with tests to verify gain/pan behavior doesn't regress.
