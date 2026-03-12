# LowL Audio - Consolidated Review & Action Plan

> Combined from `claude_mem.md` (Claude) and `codex_mem.md` (Codex) reviews, deduplicated and unified. Review date: 2026-03-12.

---

## Issue Tracker

| # | Issue | Severity | Status | Action |
|---|-------|----------|--------|--------|
| 1 | ChannelConverter `Error` passed by value | Critical | Open | Change to `Error &error` in both overloads |
| 2 | Unsupported formats return empty data without error | Critical | Open | Set error on all unimplemented decode/convert paths |
| 3 | Opus decoder silent truncation (stereo) | Critical | Open | Track decoded frames per channel, not interleaved count |
| 4 | ReSamplerSource drains entire source per `read()` | High | Open | Feed frames incrementally until resampler produces output |
| 5 | r8brain resampler memory leaks (`new` without `delete`) | High | Open | Replace raw allocations with RAII (`unique_ptr`, `vector`) |
| 6 | AudioFrame hard-coded stereo / false multi-channel contract | High | Open | Either scope API to mono/stereo or implement N-channel frames |
| 7 | Vector reallocation in mixer audio callback | Medium-High | Open | Use pre-allocated fixed-size array or double-buffer swap |
| 8 | 24-bit sample conversion wrong (byte shift + mask) | Medium | Open | Fix byte ordering in `write_sample` for INT_24 |
| 9 | `read_frames()` copies large vectors by value | Medium | Open | Use `&&` move semantics or `const &` |
| 10 | MP3 decoder loop can read past EOF / stall | Medium | Open | Clamp read size to remaining bytes, handle `frame_bytes == 0` |
| 11 | OGG `SEEK_END` adds instead of assigns | Medium | Open | Change `+=` to `=` in seek callback |
| 12 | Same sound can't overlap itself in AudioSpace | Medium | Open | Separate immutable sample storage from playback cursors |
| 13 | `Lib::terminate()` doesn't reset state | Medium | Open | Clear drivers, reset `initialized` flag |
| 14 | WASAPI driver never assigns `default_device` | Medium | Open | Set `default_device` during enumeration |
| 15 | `get_closest_properties()` is a stub | Medium | Open | Implement scoring/matching against requested properties |
| 16 | `AudioSpace::clear_all_audio()` off-by-one ID | Medium | Open | Start `current_id` at 0 or don't insert null entry |
| 17 | Panning formula increases energy when panned | Low-Medium | Open | Replace with equal-power cos/sin panning law |
| 18 | ErrorCode sign inconsistency (205, 206 positive) | Low-Medium | Open | Make all error codes negative or fix `has_error()` check |
| 19 | AudioData constructor copies frame vector at some call sites | Low-Medium | Open | Add `std::move()` at call sites |
| 20 | Static initialization order fiasco in `Lib` | Low | Open | Use function-local statics (Meyers' singleton) |
| 21 | AudioFrame missing `noexcept` on move ops | Low | Open | Remove user-defined copy ctor or add `noexcept` |
| 22 | SpaceId `uint16_t` overflow after 65535 IDs | Low | Open | Add overflow detection or use `uint32_t` |
| F1 | ~~Mixer clipping / normalization~~ | ~~High~~ | **Fixed** | Peak normalization with `set_normalize_output()` toggle |
| F2 | ~~Silence fill buffer overrun~~ | ~~Critical~~ | **Fixed** | Replaced with `std::memset` |
| F3 | ~~`StreamWriteFailed` missing in switch~~ | ~~High~~ | **Fixed** | Added case to `Error::to_error_text()` |

---

## Project Overview

**LowL Audio** is a C++17 low-latency audio library for real-time applications (games, DAWs, interactive media). Platform-agnostic abstraction over CoreAudio (macOS) and WASAPI (Windows) with a composable `AudioSource` pipeline, lock-free threading, and multi-format codec support (WAV/MP3/FLAC/OGG/Opus).

~75 source files, 9 third-party dependencies (all permissive), CMake 3.31+.

```
Platform APIs (CoreAudio, WASAPI)
    -> AudioDevice (hardware abstraction)
        -> AudioSource pipeline (composable decorator chain)
            -> AudioData (pre-loaded buffers)
            -> AudioStream (lock-free SPSC queue streaming)
            -> AudioMixer (blend N sources, lock-free event queue)
            -> AudioSpace (game-oriented ID-based sound manager)
            -> ReSamplerSource (on-the-fly rate conversion)
        -> Readers (WAV, MP3, FLAC, OGG, Opus -> AudioData)
        -> Converters (sample format, channel layout, sample rate)
```

---

## Strengths

1. **Lock-free real-time design.** `AudioMixer` uses `moodycamel::ConcurrentQueue` for event-driven source management. `AudioStream` uses `ReaderWriterQueue` for SPSC streaming. Volume/panning are atomics validated with `static_assert(is_always_lock_free)`. This is the most important design decision for RT audio and it's done right.

2. **Composable AudioSource pipeline.** The decorator/chain pattern is elegant and extensible. Adding effects, filters, or spatial audio is just another `AudioSource` wrapper. Both reviews independently highlighted this as the architectural strength.

3. **Platform abstraction quality.** CoreAudio and WASAPI backends are well-structured with factory construction (`_constructor_tag` pattern), error propagation with vendor codes, and correct API usage. Compile-time driver selection keeps binary size lean.

4. **AudioSpace as pre-processed sound bank.** Eagerly resamples and converts all audio to a uniform format at load time, providing zero-overhead ID-based playback. A distinctive and practical game-audio design.

5. **Type safety.** Strong typedefs (`Sample`, `Volume`, `Panning`, `SampleRate`, `SpaceId`), scoped enums, configurable `LOWL_TYPE_SAMPLE_64` for double-precision pipelines.

6. **Strict build configuration.** Unusually strict CMake warning profile for a project this size. `-Werror -Wswitch` catches real bugs.

---

## Novel Ideas

- **Protected constructor tag pattern** (`_constructor_tag` struct) prevents public construction while allowing factory methods without `friend` declarations.
- **`atomic_flag` for seek/reset sync** -- lightweight dirty bit between control and audio threads, avoiding mutexes on the hot path.
- **`ReleasePool` with timer-based cleanup** -- defers `shared_ptr` destruction off the RT thread by periodically pruning objects with `use_count == 1`.
- **Mixer command queue** -- good foundation for a fully RT-safe command model. Already the right instinct.

---

## Critical Issues (Open)

### 1. ChannelConverter `Error` Passed by Value (Critical)

**Files:** `lowl_audio_channel_converter.h:24,27`, `lowl_audio_channel_converter.cpp`

Both overloads of `convert()` take `Error` by value. Errors set inside the function are silently lost. `AudioSpace::add_audio()` checks `error.has_error()` but will never see the error, so it relabels unconverted frames with the wrong channel tag.

```cpp
// BUG: should be Error &error
std::vector<AudioFrame> convert(..., Error error) const;
std::unique_ptr<AudioData> convert(..., Error error) const;
```

**Fix:** Change both to `Error &error`.

---

### 2. Unsupported Formats Return Empty Without Error (Critical)

**Files:** `lowl_audio_reader.cpp:46-102`, `lowl_audio_reader_wav.cpp:66-121`

Unsupported sample formats (24-bit PCM, unsigned 8-bit) and channel layouts return empty frame vectors without setting an error. The library "succeeds" while producing silence.

**Fix:** Set `UnsupportedAudioFormat` (or a more specific error) on every unimplemented decode path. Never return a valid `AudioData` unless decoding actually happened.

---

### 3. Opus Decoder Silent Truncation (Critical)

**File:** `lowl_audio_reader_opus.cpp:35-46`

`op_pcm_total()` returns PCM samples per channel, but the loop counter increments by interleaved sample count (`samples_read_per_channel * channel_count`). Stereo files stop decoding early.

**Fix:** Track decoded frames per channel, not interleaved sample count.

---

### 4. ReSamplerSource Drains Entire Source Per Read (High)

**File:** `lowl_audio_re_sampler_source.cpp:6-16`

`read()` drains *all* frames from the source in a single call, then tries to read one output frame. For a 3-minute song at 48kHz, that's ~8.6M frames processed in one audio callback, causing a massive buffer underrun.

**Fix:** Feed frames incrementally -- try resampler output first, then feed source frames one at a time until the resampler produces output:

```cpp
ReadResult ReSamplerSource::read(AudioFrame &audio_frame) {
    if (re_sampler->read(audio_frame)) return ReadResult::Read;
    AudioFrame frame{};
    while (true) {
        ReadResult result = audio_source->read(frame);
        if (result != ReadResult::Read) {
            re_sampler->flush();
            return re_sampler->read(audio_frame) ? ReadResult::Read : result;
        }
        re_sampler->write(frame);
        if (re_sampler->read(audio_frame)) return ReadResult::Read;
    }
}
```

---

### 5. r8brain Resampler Memory Leaks (High)

**File:** `lowl_audio_re_sampler_r8b.cpp:15, 107-133`

`resample_queue` is allocated with raw `new` and never released. `sample_out_ptr` is allocated with `new[]` per channel per `resample()` call and never deleted.

**Fix:** Use `std::unique_ptr<ReaderWriterQueue<AudioFrame>>` and `std::vector<double>`.

---

### 6. AudioFrame Hard-Coded Stereo (High / Architectural)

**Files:** `lowl_audio_frame.h:7-38`, `lowl_audio_channel.h`, `lowl_audio_source.cpp:59-84`

`AudioFrame` only stores `left` and `right`. `operator[]` returns `right` for any index > 0. The API exposes `Quadraphonic` and wider layouts but they are structurally unsupported. Backend property probing can expose multi-channel configs, making failure modes unpredictable.

**Fix:** Either explicitly scope the library to mono/stereo (remove misleading enums) or replace `AudioFrame` with a fixed-capacity channel array.

---

## Medium Issues (Open)

### 7. Vector Reallocation in Mixer Audio Callback (Medium-High)

**File:** `lowl_audio_mixer.cpp:20-27, 62-64`

`sources.push_back()` and `sources.erase()` can trigger heap allocation on the audio thread, causing priority inversion.

**Fix:** Pre-allocated fixed-size array (`std::array<..., MAX_SOURCES>`) or atomic double-buffer swap.

---

### 8. 24-bit Sample Conversion Wrong (Medium)

**File:** `lowl_audio_sample_converter.h:88-94`

`write_sample` for INT_24 writes bytes 1-3 (bits 8-31) instead of bytes 0-2 (bits 0-23). Byte 3 is always 0 after the 24-bit mask. The low byte of actual sample data is lost.

**Fix:** Write little-endian: `sample & 0xFF`, `(sample >> 8) & 0xFF`, `(sample >> 16) & 0xFF`.

---

### 9. Vector Copy-by-Value in Readers/Converters (Medium)

**Files:** `lowl_audio_reader.h:43-48`, `lowl_audio_channel_converter`

`read_frames()` takes `std::vector<float>` by value (~60MB for a 3-min WAV). `ChannelConverter::convert()` takes `std::vector<AudioFrame>` by value.

**Fix:** Use `std::vector<float> &&` or `const std::vector<float> &`.

---

### 10. MP3 Decoder EOF / Stall (Medium)

**File:** `lowl_audio_reader_mp3.cpp:67-81`

Loop passes constant `ENCODED_BUFFER_DECODING_STEP` to `drmp3dec_decode_frame()` near EOF, potentially reading past remaining bytes. If `frame_bytes == 0`, the loop stalls.

**Fix:** Clamp read size to remaining bytes. Break on `frame_bytes == 0`.

---

### 11. OGG SEEK_END Bug (Medium)

**File:** `lowl_audio_reader_ogg.cpp:29-40`

`SEEK_END` does `src->index += src->length + offset` instead of `src->index = src->length + offset`.

**Fix:** Change `+=` to `=`.

---

### 12. Same Sound Can't Overlap Itself (Medium)

**Files:** `lowl_audio_space.cpp:84-111`, `lowl_audio_mixer.cpp:20-27`

Mixing the same `AudioData` removes the old instance first. UI clicks, gunshots, and stacked SFX can't overlap.

**Fix:** Separate immutable sample storage from playback cursors. Create lightweight playback instances that reference shared sample data.

---

### 13-16. Smaller Medium Issues

- **`Lib::terminate()` doesn't reset state** (`lowl.cpp:25-47`) -- library not reinitializable within one process.
- **WASAPI `default_device` never assigned** (`lowl_audio_wasapi_driver.cpp:24-87`) -- `get_default_device()` broken on Windows.
- **`get_closest_properties()` is a stub** (`lowl_audio_device.cpp:22-35`) -- always returns first property, no real format negotiation.
- **`clear_all_audio()` off-by-one** (`lowl_audio_space.cpp:71-76`) -- resets `current_id` to 1 then inserts null, so next real audio starts at ID 2.

---

## Low-Severity Issues (Open)

| Issue | File | Fix |
|-------|------|-----|
| Panning energy increase | `lowl_audio_source.cpp:66-84` | Equal-power cos/sin panning law |
| ErrorCode sign inconsistency (205, 206) | `lowl_error.h` | Make all codes negative or fix `has_error()` |
| AudioData ctor frame copy at call sites | Various callers | Add `std::move(frames)` |
| Static init order fiasco in `Lib` | `lowl.cpp` | Function-local statics (Meyers' singleton) |
| AudioFrame missing `noexcept` move | `lowl_audio_frame.h` | Remove user-defined copy ctor or add `noexcept` |
| SpaceId `uint16_t` overflow | `lowl_audio_space` | Overflow detection or `uint32_t` |
| `AudioMixerEvent::type` raw `uint8_t` | `lowl_audio_mixer_event.h` | Use `enum class` |
| `_INLINE_` forces `always_inline` everywhere | `lowl_typedef.h` | Use regular `inline` on cold paths |
| CMake 3.31 minimum too high | `CMakeLists.txt` | Target 3.16-3.22 for broader adoption |
| `seek_frame` unsigned underflow when `size == 0` | `lowl_audio_data` | Guard `size == 0` before `clamp` |
| `sample_to_int8` variable named `int16` | `lowl_audio_sample_converter.h` | Rename |

---

## Applied Fixes

### F1: Mixer Clipping / Normalization -- FIXED

**Files:** `lowl_audio_mixer.h`, `lowl_audio_mixer.cpp`

Added post-sum peak normalization with an atomic toggle:

```cpp
if (normalize_output.load(std::memory_order_relaxed)) {
    Sample peak = std::max(std::abs(audio_frame.left), std::abs(audio_frame.right));
    if (peak > AudioFrame::MAX_SAMPLE_VALUE) {
        Sample attenuation = AudioFrame::MAX_SAMPLE_VALUE / peak;
        audio_frame.left *= attenuation;
        audio_frame.right *= attenuation;
    }
}
```

- `std::atomic<bool> normalize_output{true}` -- on by default, `memory_order_relaxed`
- `set_normalize_output(false)` disables when a downstream DSP limiter exists
- Preserves stereo balance. See `CRIT-2.md` for full 3-fix analysis.

**Future:** When a DSP effects chain with a limiter is added, call `mixer->set_normalize_output(false)` to avoid double gain reduction.

### F2: Silence Fill Buffer Overrun -- FIXED

**File:** `lowl_audio_device.cpp:80`

Old nested loop wrote `channels * bytes_per_frame` bytes per frame (buffer overrun). Replaced with `std::memset(p_dst, 0, missing_frames * p_bytes_per_frame)`. Regression test added in `test_audio_device.cpp`.

### F3: StreamWriteFailed Enum -- FIXED

**File:** `lowl_error.h`, `lowl_error.cpp`

`StreamWriteFailed` was missing from `Error::to_error_text()` switch, breaking build under `-Werror -Wswitch`.

---

## Features That Would Make Good Additions

1. **Effects/DSP Pipeline** -- `AudioEffect` base class (gain, EQ, reverb, compressor, limiter) as `AudioSource` wrappers. Two levels: per-source and post-mix per bus. When a post-mix limiter exists, disable `normalize_output` on the mixer.
2. **Voice Instances / Polyphony** -- Separate immutable sample storage from playback cursors so the same sound can overlap. Simplifies `AudioSpace` and enables proper SFX stacking.
3. **3D Spatial Audio** -- Distance attenuation, HRTF, doppler. Extends the existing panning system to 3D coordinates.
4. **Audio Bus/Group System** -- Named buses (SFX, Music, Voice, Master) with per-bus volume/effects. `AudioSpace` is close but lacks hierarchical routing.
5. **Streaming Decoder** -- Decode from disk in chunks for long music tracks instead of loading entire files into memory.
6. **Linux Backend** -- ALSA/PulseAudio/PipeWire. Only major desktop platform without support.
7. **Looping with Loop Points** -- Loop forever, N times, or between points A-B. Essential for game audio.
8. **Fade In/Out and Crossfade** -- Programmatic envelope control. Current instantaneous volume changes cause clicks.
9. **Event/Callback System** -- Playback finished, loop point reached, buffer underrun, device hot-plug.
10. **Resampler/Converter Test Fixtures** -- Golden-file tests with expected frame counts. Currently untested.

---

## Strategic Recommendations

### 1. Tighten the public contract

The code suggests "general audio engine" but the implementation is "mono/stereo playback with in-progress multi-channel scaffolding". Either explicitly scope to mono/stereo or finish N-channel support before exposing those enums. Narrowing the contract is cheaper than debugging silent wrong-channel behavior.

### 2. Make failure impossible to ignore

Recurring theme across both reviews: silent degradation (empty frames, swallowed errors, partial decode, stubs that return success). Never return a valid `AudioData` when decode/conversion didn't succeed. Use `Error &` consistently. Add assertions on internal paths, explicit errors on public paths.

### 3. Separate sample data from playback state

`AudioData` owns both frames and cursor. This blocks overlap, cloning, and reuse. Store PCM once, create lightweight playback instances that reference shared storage. Enables polyphony, simplifies `AudioSpace`, reduces copies.

### 4. Use RAII consistently in decode/convert code

Smart pointers are used in most places, but raw `new`/`new[]` persists in the resampler and backend helpers -- exactly where leaks are most expensive under repeated asset loads.

### 5. Rebuild the test matrix

Current tests prove two tiny primitives work. Missing: decoder golden tests for all formats, resampler correctness, channel conversion failures, `AudioSpace` behavior, mixer edge cases, backend negotiation, and a driver test strategy that doesn't depend on Core Audio startup.

---

## Conclusion

The core architecture is strong. The `AudioSource` decorator pattern, lock-free RT design, and `AudioSpace` concept are the right foundations. The weakness is **correctness hardening at the edges**: silent failures, format misrepresentation, memory leaks in converters, and missing tests.

**Priority order for maximum impact:**

1. **Fix issues #1-3 (Critical)** -- error propagation and decoder correctness. These cause silent data corruption.
2. **Fix issues #4-5 (High)** -- resampler performance and memory leaks. These cause audible glitches and resource exhaustion.
3. **Resolve #6 (High/Architectural)** -- decide mono/stereo scope vs N-channel. This unblocks everything else.
4. **Fix #7-8 (Medium-High/Medium)** -- RT safety and 24-bit output. These affect professional audio use.
5. **Build test fixtures** -- golden-file decode tests, resampler verification, mixer edge cases.
6. **Add DSP pipeline** -- unlocks effects, proper limiting (replace mixer normalization toggle), and bus routing.
