# lowl_audio - Code Review

## Project Overview

**lowl_audio** is a cross-platform C++17 audio library providing a unified interface over native audio backends (CoreAudio on macOS, WASAPI on Windows). It handles audio decoding (WAV, MP3, FLAC, OGG, Opus), sample rate conversion, channel conversion, and real-time mixing through a lock-free audio pipeline. The library is designed as a Godot engine module (`gd_lowl_audio`) but is architecturally independent.

### Architecture at a glance

```
User Code
  │
  ▼
AudioSpace / AudioMixer          ← high-level management
  │
  ├─ AudioData                   ← pre-loaded sample buffers
  ├─ AudioStream                 ← real-time lock-free queue
  │
  ▼
AudioDevice.write_frames()       ← sample conversion + clamping
  │
  ▼
CoreAudio / WASAPI callback      ← OS real-time audio thread
```

All audio sources inherit from `AudioSource`, which provides a pull-based `read(AudioFrame&)` interface. The device's real-time callback pulls frames through the source graph on each buffer cycle.

### Key types

| Type | Underlying | Purpose |
|------|-----------|---------|
| `Sample` | `float` (default) or `double` (`LOWL_TYPE_SAMPLE_64`) | Single audio sample value |
| `AudioFrame` | `{Sample left, right}` | Stereo sample pair |
| `SampleRate` | `double` | Hz |
| `Volume` | `Sample` | Linear gain multiplier |
| `Panning` | `Sample` | [-1, 1] left-to-right |
| `SpaceId` | `uint16_t` | ID for sounds in an AudioSpace |

---

## What Is Good

### 1. Lock-free real-time audio path

The entire audio callback path is free of locks and allocations. Parameter changes (volume, panning, play/pause) use `std::atomic`. Mixer add/remove operations go through `moodycamel::ConcurrentQueue` so the main thread never blocks the audio thread. This is the correct approach for real-time audio — most audio libraries get this wrong.

### 2. Clean pull-based source graph

The `AudioSource::read(AudioFrame&) -> ReadResult` abstraction is simple and composable. `AudioData`, `AudioStream`, `AudioMixer`, and `AudioSpace` all implement the same interface, so they can be freely nested: a mixer can mix other mixers, a space wraps a mixer internally, etc. The `ReadResult` enum (`Read`, `Pause`, `End`, `Remove`) cleanly communicates source lifecycle without exceptions or error codes in the hot path.

### 3. Equal-power panning law

```cpp
audio_frame.left  *= std::sqrt(1.0 - pan);
audio_frame.right *= std::sqrt(1.0 + pan);
```

This is the correct perceptual panning law. Many hobby audio engines use linear panning which causes a perceived volume dip at center. Good choice.

### 4. Thorough device capability probing (CoreAudio)

`CoreAudioDevice::create_device_properties()` actually tests whether the device supports each sample rate / format combination by attempting to set the stream format on a temporary AudioUnit. This is more robust than trusting device-reported capabilities, which are often wrong on macOS.

### 5. Comprehensive format support

Five file formats (WAV, MP3, FLAC, OGG, Opus), seven sample formats (float32/64, int8/16/24/32, uint8), and high-quality resampling via r8brain-free-src. The decoders are header-only (dr_libs) or well-established C libraries, keeping the dependency surface reasonable.

### 6. Proper silence fill on buffer underrun

In `AudioDevice::write_frames()`, when the source runs dry mid-buffer the remaining frames are zero-filled. This prevents pops/garbage from reaching the output — a detail many audio engines skip.

### 7. Constructor tag pattern for controlled construction

`AudioDevice` uses a `_constructor_tag` struct to force construction through factory methods (`construct()`), while still allowing `make_unique`. Clean alternative to private constructors + friend classes.

### 8. CoreAudio callback uses authoritative frame count

The callback uses `inNumberFrames` (the OS-provided frame count) directly instead of re-deriving it from buffer byte size. It also validates that the system buffer is large enough before writing, returning `kAudio_ParamError` if not. Derives `bytes_per_frame` from its own configured device properties rather than trusting `mNumberChannels` from the buffer list.

---

## What Is Bad / Issues

### 1. ~~`double` as the internal sample format — unnecessary cost~~ (FIXED)

`Sample` now defaults to `float` with an opt-in `LOWL_TYPE_SAMPLE_64` define for `double`. `Volume` and `Panning` are also tied to `Sample` instead of being independently `double`. Explicit `static_cast<Sample>(...)` is used throughout (panning, resampler output, channel conversion) to ensure clean compilation under either mode.

**Remaining concern:** `SampleRate` and `TimeSeconds` are still `double_l` regardless of sample type — this is correct since sample rates need full precision for resampling math. However, `std::atomic<Sample>` should be verified as lock-free on target platforms when using `float` (it typically is on x86/ARM64, but worth a `static_assert`).

### 2. `AudioFrame` is hardcoded to stereo

`MAX_CHANNEL = 2` and the struct literally has `left` and `right` fields. The comments describe quad and 5.1 layouts, but no implementation exists. The panning switch statement is identical for Mono, Stereo, and Quadraphonic. This creates a false promise of multi-channel support.

**Recommendation:** Either remove the multi-channel comments/enums or implement a channel-count-parameterized frame (e.g., `std::array<Sample, N>` or a small fixed buffer).

### 3. ~~`AudioMixer::read()` has a subtle bug in the Remove path~~ (FIXED)

The `&&` was corrected to `||`. The bounds check now works as intended. The pointer-offset approach to computing the index from a range-for reference is still fragile — an explicit index-based loop or `std::distance` would be clearer.

### 4. `get_closest_properties()` is unimplemented

```cpp
for (AudioDeviceProperties property: properties_list) {
}
// TODO find best match
return properties_list[0];
```

This always returns the first property set. If the user requests a specific sample rate or format, the library silently ignores the request and returns whatever is first. This will cause unexpected resampling or format mismatches.

### 5. `AudioDevice::write_frames()` silence fill is buggy

```cpp
unsigned long missing_samples = missing_frames * (unsigned long) audio_source->get_channel_num();
for (; current_sample < missing_samples; current_sample++) {
    for (unsigned long frame_byte = 0; frame_byte < p_bytes_per_frame; frame_byte++) {
        *remaining++ = 0;
    }
}
```

The outer loop iterates per-sample, but the inner loop writes `p_bytes_per_frame` (which is `sample_size * num_channels`) bytes per iteration. This writes `num_channels` times too many zero bytes. For stereo 32-bit float: should write `missing_frames * 8` bytes, actually writes `missing_frames * 16` — buffer overrun.

### 6. ~~`AudioData` copies the entire frame vector in the constructor~~ (FIXED)

Now uses `std::move` in the member initializer list:
```cpp
AudioData(...) : AudioSource(...), frames(std::move(p_audio_frames)) { ... }
```

### 7. `ReleasePool` uses a mutex — defeating the lock-free design

The release pool's `add()` takes a `std::lock_guard<std::mutex>`. If called from or near the audio thread path (e.g., when a source finishes and gets cleaned up), this introduces a potential priority inversion. The 10-second timer callback also locks the same mutex on a background thread.

### 8. No volume clamping or smoothing

Volume is applied as a raw multiplier with no clamping. Values > 1.0 cause clipping. Values that change suddenly (e.g., from 0 to 1) cause clicks/pops because there's no per-frame interpolation (ramping). The same applies to panning changes. In professional audio, parameter changes are always smoothed over a small window (typically 1-10ms).

### 9. `CoreAudioDevice::stop()` ignores all OSStatus results

```cpp
void CoreAudioDevice::stop(Error &error) {
    OSStatus result = noErr;
    result = AudioOutputUnitStop(audio_unit);
    result = AudioUnitReset(audio_unit, kAudioUnitScope_Global, 0);
}
```

Both return values are silently discarded, and the `error` parameter is never used.

### 10. Error handling by mutable reference is verbose and error-prone

Every function takes `Error &error` and callers must check `error.has_error()` after every call. This leads to deeply nested early-return chains (see `CoreAudioDevice::start()` — 8 sequential error checks). There's no mechanism to prevent using a stale error object that already contains an error from a previous call. A `Result<T, Error>` pattern or C++ exceptions would be cleaner.

### 11. `ErrorCode` has inconsistent signedness

```cpp
ReaderNotFound = 205,   // positive
ReaderNoAudioData = 206, // positive
// everything else is negative
```

Two error codes are positive, breaking the convention. This could cause issues if code checks `error_code < 0` to detect errors.

---

## Novel / Interesting Ideas

### 1. AudioSpace as a sound bank abstraction

The `AudioSpace` concept — a named collection of sounds managed by integer ID with automatic resampling on `add_audio()` — is a practical high-level API for game audio. It maps well to common game patterns: load a set of sound effects, play them by ID, manage volume/panning per-sound. This is a layer above what most low-level audio libraries provide, and below what full audio middleware (FMOD, Wwise) offers. Good middle-ground.

### 2. Event-driven mixer modification

Using a concurrent queue of `AudioMixerEvent` (Mix/Remove) to modify the source list is a clean pattern. The audio thread processes events at the top of each `read()` call, ensuring modifications are applied at frame boundaries. This avoids the common pitfall of locking a source list or using double-buffering schemes.

### 3. `ReadResult::Remove` as a self-cleanup signal

Sources can signal their own removal from the mixer by returning `Remove`. This pushes lifecycle management into the source itself (e.g., AudioData returns `Remove` when it reaches the end), rather than requiring the mixer to poll source state. Elegant for fire-and-forget sounds.

### 4. Device capability testing via trial configuration

Rather than parsing opaque device capability flags, the library actually attempts to configure a temporary AudioUnit with each format/rate combination and checks if it succeeds. This empirical approach is more reliable than declarative capability queries, which are notoriously unreliable across macOS audio hardware.

### 5. Atomic flag for seek synchronization

`AudioData` uses `std::atomic_flag is_not_reset` as a signal between the main thread (which calls `seek_time`/`reset`) and the audio thread (which calls `read`). The audio thread does `test_and_set()` — if the flag was cleared, it knows a seek was requested and applies it. This is a minimal, allocation-free synchronization primitive for cross-thread seek commands.

---

## Summary

| Aspect | Rating | Notes |
|--------|--------|-------|
| Architecture | Good | Clean pull-model, composable sources, lock-free hot path |
| Real-time safety | Good | Atomics, lock-free queues, no allocations in callback |
| API design | Mixed | AudioSpace is well-designed; Error-by-ref is verbose; some unfinished stubs |
| Correctness | Improved | Mixer bounds check and AudioData double-copy fixed; silence fill buffer overrun still present |
| Performance | Improved | Configurable float/double sample type; `static_cast` at conversion boundaries; no SIMD mixing yet |
| Multi-channel | Incomplete | Stereo only despite quad/5.1 enums and comments |
| Platform support | Partial | CoreAudio is solid; WASAPI exists but less tested; no Linux (ALSA/PulseAudio/PipeWire) |
| Test coverage | Low | Only a few doctest cases; no integration/stress tests |

The library demonstrates strong understanding of real-time audio constraints (lock-free design, atomic parameters, buffer underrun handling). Recent changes addressed the most critical issues: the mixer bounds check bug, the AudioData double-copy, the hardcoded double sample type (now configurable float/double), and the CoreAudio callback reliability. The main remaining risks are the silence fill buffer overrun in `write_frames()`, the unimplemented `get_closest_properties()`, and the lack of parameter smoothing. The AudioSpace abstraction and event-driven mixer are well-conceived patterns worth building on.
