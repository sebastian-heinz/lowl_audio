# Codex Memory: `lowl_audio`

## Review Scope

- Review date: 2026-03-12
- Method: source inspection across `src/`, `test/`, `demo/`, `README.md`, and build files
- Limitation: the packaged CMake build could not be run because `cmake` is not installed in this environment
- Verification performed: a direct `clang++` build of the self-contained `AudioData` and `AudioStream` doctests passed

## Project Overview

`lowl_audio` is a small cross-platform C++ audio playback library with:

- output backends for CoreAudio and WASAPI
- file decoding for WAV, MP3, FLAC, OGG, and Opus
- a pull-based audio graph centered on `AudioSource`
- source types for in-memory audio (`AudioData`), queued audio (`AudioStream`), mixing (`AudioMixer`), and ID-based playback (`AudioSpace`)
- resampling through r8brain and basic channel/sample-format conversion utilities

The architectural center is good: backends ask an `AudioSource` for frames, and everything else is composed around that abstraction.

## What Is Good

- The `AudioSource` abstraction is strong. `AudioData`, `AudioStream`, `AudioMixer`, and `AudioSpace` all fit the same pull contract cleanly.
- The library is split along sensible subsystem boundaries: decode, convert, source graph, backend, and API entry point.
- `AudioSource` keeps volume and panning in atomics, which is a pragmatic choice for low-cost runtime control.
- `AudioMixer` uses an event queue instead of mutating the source list directly from external threads. That is the right instinct for audio code.
- `AudioSpace` is a useful game-audio convenience layer. ID-based playback is a better UX than forcing callers to manage source lifetimes themselves.
- Dependency strategy is practical. Vendor code is already in-tree, which lowers setup friction.
- The CMake warning profile is unusually strict for a hobby-sized audio project, which is a positive sign.

## Novel Or Interesting Ideas

- `AudioSpace` is the most distinctive feature in the repo. It acts like a lightweight asset registry plus playback surface.
- The mixer command queue is a good foundation for moving toward a more explicit real-time safe command model.
- The project sits in an interesting middle ground: more structured than a thin backend wrapper, but smaller and easier to reason about than a full audio engine.

## What Is Bad

The main weakness is not the broad design. It is trustworthiness at the edges:

- format support is advertised more broadly than it is actually implemented
- several correctness bugs exist in decoding and buffer handling
- some APIs silently succeed when they should fail
- documentation and tests are behind the current code

This means the project is promising, but not yet dependable as a foundation for production audio playback.

## Critical Source Analysis

### High-Severity Issues

1. `src/audio/backend/lowl_audio_device.cpp:76-87`

The silence-fill path writes `p_bytes_per_frame` bytes once per missing sample instead of once per missing frame. For stereo float32, this doubles the required write size and can overrun the destination buffer.

Why it matters:
- this runs in the device callback path
- buffer overruns in audio backends are the kind of bug that turn into random noise, crashes, or nondeterministic corruption

Safer shape:

```cpp
const auto sample_bytes = get_sample_size_bytes(audio_device_properties.sample_format);
const auto silence_bytes =
    missing_frames * audio_source->get_channel_num() * sample_bytes;
std::memset(p_dst, 0, silence_bytes);
```

2. `src/audio/lowl_audio_frame.h:7-38`, `src/audio/lowl_audio_channel.h:7-12`, `src/audio/source/lowl_audio_source.cpp:59-84`, `src/audio/reader/lowl_audio_reader.cpp:114-143`

The API claims support for `Quadraphonic`, channel masks, and wider layouts, but `AudioFrame` only stores `left` and `right`, and `operator[]` returns `right` for every channel index other than `0`. That means channel counts above stereo are structurally unsupported.

Why it matters:
- it creates a false contract between public API and actual behavior
- multi-channel audio is not just unsupported, it is misrepresented
- backend property probing can still expose multi-channel configurations, which makes failure modes harder to predict

Better direction:
- either narrow the public contract to mono/stereo only
- or replace `AudioFrame` with a fixed-capacity array or dynamic channel buffer that matches the declared channel model

3. `src/audio/convert/lowl_audio_channel_converter.cpp:26-59`

`Error` is passed by value, not by reference, in both conversion overloads. Unsupported conversions therefore do not propagate failure to callers.

This is especially dangerous in `AudioSpace::add_audio()` because the result is then relabeled with the destination channel even when conversion failed.

Why it matters:
- callers think conversion worked
- the returned `AudioData` can contain unconverted frames with a false channel label

Minimal fix:

```cpp
std::vector<AudioFrame> convert(..., Error& error) const;
std::unique_ptr<AudioData> convert(..., Error& error) const;
```

4. `src/audio/reader/lowl_audio_reader.cpp:46-102`, `src/audio/reader/lowl_audio_reader_wav.cpp:66-121`

Unsupported sample formats and channel layouts often return empty frame vectors without setting an error. WAV maps 24-bit PCM and unsigned 8-bit PCM, but the generic frame reader does not actually decode those formats.

Why it matters:
- the library can "succeed" while returning silent or zero-length audio
- failures become data-quality bugs instead of explicit errors

Required change:
- whenever a format/layout is not implemented, set `UnsupportedAudioFormat` or a more specific error
- do not return a valid `AudioData` unless decoding actually happened

5. `src/audio/reader/lowl_audio_reader_opus.cpp:35-46`

`op_pcm_total()` returns PCM samples per channel, but the loop counter is incremented by interleaved sample count (`samples_read_per_channel * channel_count`). Stereo files can therefore stop decoding early.

Why it matters:
- this is silent truncation
- multi-channel decode bugs are easy to miss without golden-file tests

Correct approach:
- track decoded frames per channel, not interleaved sample count

6. `src/audio/convert/lowl_audio_re_sampler_r8b.cpp:15`, `src/audio/convert/lowl_audio_re_sampler_r8b.cpp:107-133`

`resample_queue` is allocated with raw `new` and never released. `sample_out_ptr` is allocated with `new[]` per channel and never deleted in `resample()`.

Why it matters:
- every resample leaks memory
- repeated asset loading will fragment and grow process memory unnecessarily

Use RAII consistently:
- `std::unique_ptr<ReaderWriterQueue<AudioFrame>>`
- `std::vector<double> sample_out(expected_frames)`

### Medium-Severity Issues

1. `src/audio/reader/lowl_audio_reader_mp3.cpp:67-81`

The loop condition is `while (bytes_read <= p_size)` while always passing a constant `ENCODED_BUFFER_DECODING_STEP` to `drmp3dec_decode_frame()`. Near EOF this can read beyond the actual remaining byte count, and if `frame_bytes` becomes `0`, the loop can stop making progress.

2. `src/audio/reader/lowl_audio_reader_ogg.cpp:29-40`

`SEEK_END` handling is wrong. It does `src->index += src->length + offset` instead of assigning `src->length + offset`.

3. `src/audio/source/lowl_audio_stream.cpp:3-29`, `src/audio/lowl_audio_utilities.cpp:3-10`

`AudioStream` has a hardcoded queue size of `100`. `Utilities::to_stream()` writes an entire `AudioData` into that queue and ignores failed writes, so anything larger than 100 frames is truncated.

4. `src/audio/source/lowl_audio_space.cpp:71-76`

`clear_all_audio()` resets `current_id` to `1` and then inserts a null entry, which causes the next real audio to start at ID `2` instead of `1`.

5. `src/audio/source/lowl_audio_space.cpp:84-111`, `src/audio/source/lowl_audio_mixer.cpp:20-27`

Mixing the same `AudioData` twice removes the old instance first. That means the same sound cannot overlap with itself. For UI clicks, gunshots, or stacked SFX, this is a real limitation.

6. `src/lowl.cpp:25-47`

`Lib::terminate()` does not clear driver state or reset the `initialized` flag, so the library is not meaningfully reinitializable within one process.

7. `src/audio/backend/wasapi/lowl_audio_wasapi_driver.cpp:24-87`

The WASAPI driver enumerates devices but never assigns `default_device`, which weakens `Lib::get_default_device()` on Windows.

8. `src/audio/backend/lowl_audio_device.cpp:22-35`

`get_closest_properties()` is effectively a stub that always returns the first property. That means format negotiation is not real yet.

### Documentation And Testing Gaps

1. `README.md:57-91`, `README.md:103-133`, `README.md:151-163`, `README.md:190-237`, `src/audio/source/lowl_audio_space.h:17-27`

The docs do not match the current API. Examples reference nonexistent or outdated methods like `mix_data()`, `load()`, `get_mixer()`, old type names, and old `device->start(...)` signatures.

2. `test/test_audio_data.cpp:8-55`, `test/test_audio_stream.cpp:8-59`, `test/test_driver.cpp:7-31`

The tests only cover:
- basic `AudioData` reads
- basic `AudioStream` reads
- driver initialization

What is missing:
- decoder golden tests for all formats
- resampler correctness
- channel conversion failures
- `AudioSpace` behavior
- mixer edge cases
- backend negotiation and stop/start lifecycle

## Features That Would Make Good Additions

- True voice instances for `AudioSpace`
  - separate immutable sample storage from playback cursors so the same sound can overlap
- Explicit mono/stereo contract, or real N-channel frame support
  - the project needs to choose one
- Streaming decoders
  - current readers decode whole files into memory; long music tracks need streamed decode
- Resampler and converter tests with reference outputs
  - especially around tail handling and channel layout changes
- Backend/device capability ranking
  - `get_closest_properties()` should be a real scoring function
- Callback-safe command ring for all state changes
  - the mixer is already close to this idea
- Loop regions, pitch control, and fades
  - all are natural extensions of the existing source model

## Suggestions On Improvement

### 1. Tighten the public contract first

Right now the code suggests "general audio engine" while the real implementation is "mono/stereo playback engine with some in-progress multi-channel scaffolding".

Recommendation:
- either explicitly scope the library to mono/stereo
- or finish the multi-channel implementation before exposing those enums in public APIs

Why:
- narrowing the contract is cheaper than debugging undefined behavior

### 2. Make failure impossible to ignore

A recurring problem is silent degradation: empty frames, swallowed errors, partial decode, and stale docs.

Recommendation:
- never return a successful `AudioData` when decode/conversion did not actually succeed
- use `Error&` consistently
- add assertions in internal-only paths and explicit error returns in public paths

Why:
- audio bugs are hard enough when they fail loudly
- silent success makes the library feel unreliable

### 3. Separate immutable sample data from playback state

`AudioData` currently owns both the frames and the cursor. That makes overlap, cloning, and reuse awkward.

Recommendation:
- store PCM once
- have small playback instances that reference shared sample storage

Why:
- enables polyphony
- simplifies `AudioSpace`
- reduces unnecessary copies

### 4. Use RAII consistently, especially in decode/convert code

The project already uses smart pointers in many places. The remaining raw allocations are concentrated in the parts where leaks are most expensive.

Recommendation:
- remove raw `new[]` and raw owning pointers from resampler and backend helper code
- wrap C resources with local deleters where possible

Why:
- the codebase becomes easier to trust under repeated asset loads

### 5. Rebuild the test matrix around real assets and invariants

The current tests prove that two tiny primitives work, but not that the library is safe as a whole.

Recommended test buckets:
- decode fixtures for WAV/MP3/FLAC/OGG/Opus
- resample fixture tests with expected frame counts
- `AudioSpace` ID and replay semantics
- mixer overlap/removal semantics
- property negotiation and default-device behavior

## Final Assessment

This is a worthwhile codebase with a good core shape. The strongest part is the conceptual model: `AudioSource` plus a small set of composable source types is the right design center.

The weakest part is correctness hardening. The project is currently closer to "promising engine prototype" than "reliable library". If the high-severity issues above are fixed first, the rest of the library becomes much easier to evolve.
