# Hot Path Optimization Proposals

Three approaches to reducing memory-bandwidth pressure and per-sample overhead in the render chain. They are independent and can be applied in any order.

## Current render chain

```text
CoreAudio/WASAPI callback
  -> AudioDevice::render_to_device_buffer()           [clear buffer, convert/interleave]
       -> AudioMixer::render()                         [clear output block]
            -> per source:
                 scratch_buffer.clear()
                 source->render(scratch_view)           [copy from AudioData]
                 process_volume(scratch_view)            [multiply pass]
                 process_panning(scratch_view)           [sqrt + multiply pass]
                 accumulate into output block            [add pass]
            -> process_volume(output block)             [multiply pass]
            -> process_panning(output block)            [sqrt + multiply pass]
       -> per sample: clamp + switch(format) + write   [scalar conversion]
```

Every sample is touched at minimum 7 times: clear, copy, voice gain, voice pan, accumulate, mixer gain, mixer pan — then clamped and format-converted one at a time.

---

## Proposal 1: Fused gain/pan accumulation

Eliminate the separate `process_volume` and `process_panning` passes on each source by folding gain and pan into the mixer accumulation loop.

### What changes

`AudioMixer::render_mixed_block()` currently:
1. Clears scratch buffer
2. Calls `source->render(scratch_view)` which copies data and applies voice gain+pan
3. Accumulates scratch into output with a plain `+=` loop

Replace step 3 with a gain-scaled accumulate. Each source already knows its volume and panning — instead of three passes (copy, gain, pan, then accumulate) the mixer reads the source's current gain per channel and does:

```
dst[i] += src[i] * channel_gain;
```

in one loop. `process_volume()` and `process_panning()` are no longer called per-source.

For the mixer's own master gain/pan (set by `AudioSpace`), apply as a final single pass on the mixed output, or fold it into each source's channel gain if it hasn't changed since last callback (check via a dirty flag).

### Passes removed per source

| Before | After |
|--------|-------|
| copy, voice gain, voice pan, accumulate (4 passes) | copy, gain-scaled accumulate (2 passes) |

### Cached panning gains

`process_panning()` calls `std::sqrt()` every render call even when panning hasn't changed. Cache the left/right gains and recompute only when the panning value changes (detectable via a generation counter or direct comparison on the atomic).

### Files touched

- `src/audio/source/lowl_audio_mixer.cpp` — fused accumulate loop
- `src/audio/source/lowl_audio_source.h/.cpp` — expose `get_channel_gains()`, cache pan gains
- `src/audio/source/lowl_audio_voice.cpp` — skip per-voice `process_volume`/`process_panning`

---

## Proposal 2: Format-specialized device write paths

Replace the per-sample `switch(format)` + clamp in `render_to_device_buffer()` with format-specialized bulk conversion functions.

### What changes

`render_to_device_buffer()` currently iterates sample-by-sample:
```cpp
for (frame) {
    for (channel) {
        clamp(sample);                    // branch
        write_sample(format, sample);     // switch on 8 format cases
    }
}
```

Replace with a dispatch-once, loop-many pattern:

```cpp
switch (format) {
    case FLOAT_32: write_interleaved_float32(output_block, dst, produced_frames); break;
    case INT_16:   write_interleaved_int16(output_block, dst, produced_frames);   break;
    // ...
}
```

Each specialization is a tight inner loop with no branches per sample. The `FLOAT_32` path can use a direct planar-to-interleaved copy (or even `memcpy` for mono). The `INT_16` path folds the clamp into the conversion arithmetic.

### Gains

- Eliminates per-sample switch dispatch (~8 cases, each callback)
- Eliminates redundant clamp before conversion (the typed converters already clamp)
- Enables autovectorization — tight typed loops with no branches are SIMD-friendly
- `FLOAT_32` (the common case on both CoreAudio and WASAPI shared mode) becomes a simple interleave with no arithmetic

### Files touched

- `src/audio/backend/lowl_audio_device.cpp` — new specialized write functions, replace inner loop
- `src/audio/convert/lowl_audio_sample_converter.h` — bulk conversion helpers (optional, can live in device.cpp)

---

## Proposal 3: Eliminate redundant buffer clears

Three separate clears happen today for every callback. Two of them can be removed.

### Current clears

| Clear | Location | Purpose |
|-------|----------|---------|
| Device scratch buffer | `lowl_audio_device.cpp:132` | Zero the render buffer before calling source->render() |
| Mixer output block | `lowl_audio_mixer.cpp:305-308` | Zero output before accumulating sources |
| Mixer scratch buffer | `lowl_audio_mixer.cpp:205` | Zero scratch before each source renders into it |

The device clear and the mixer output clear are the same memory — the device creates a view into its render buffer and passes it to the mixer, which zeros it again. One of them is redundant.

The per-source scratch clear is necessary only because sources may produce fewer frames than requested, and the accumulation loop must not read garbage from the tail. But if the accumulation loop uses `frames_produced` as its bound (which it already does on line 208), the tail is never read.

### What changes

1. **Remove the device-level clear** (`render_buffer.clear()` at `lowl_audio_device.cpp:132`). The mixer already zeros its output block before accumulation.

2. **Remove the per-source scratch clear** (`scratch_buffer.clear()` at `lowl_audio_mixer.cpp:205`). The source writes `frames_produced` frames, and the accumulation loop already bounds itself to `min(frames_produced, block.frame_count)`. The unwritten tail is never read.

   Safety contract: sources must fully write all channels for the frames they claim to have produced. `AudioVoice::render()` and `AudioStream::render()` both satisfy this already (`std::copy_n` of exactly `frames_to_copy`).

3. **Keep the mixer output clear** (`lowl_audio_mixer.cpp:305-308`). This is the one authoritative zero-fill before accumulation begins.

### Passes removed

Two full-buffer `memset` operations per callback eliminated. For a 512-frame stereo buffer at 4 bytes/sample, that's 2 * 512 * 2 * 4 = 8 KB of unnecessary writes per callback.

### Files touched

- `src/audio/backend/lowl_audio_device.cpp:132` — remove `render_buffer.clear()`
- `src/audio/source/lowl_audio_mixer.cpp:205` — remove `scratch_buffer.clear()`
