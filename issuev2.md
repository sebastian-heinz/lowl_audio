# lowl_audio v2 -- Comprehensive Architecture & Code Review

## Scope

Full review of the `lowl_audio` core library (`src/`, `test/`, `demo/`, `c_api/`, build system).
Covers bugs, thread safety, undefined behavior, resource leaks, API design, architecture, and test gaps.

---

## Issue Index

| #  | Status | Severity | Category | Subsystem | Title |
|----|--------|----------|----------|-----------|-------|
| 1  | FIXED | Critical | Bug | Reader | WAV reader leaks `drwav` on multiple error paths |
| 2  | FIXED | Critical | Bug | Reader | FLAC reader never calls `drflac_close` |
| 3  | FIXED | Critical | Bug | Reader | FLAC / WAV integer overflow in `bytes_to_read_test` on 32-bit |
| 4  | FIXED | Critical | Bug | Converter | `sample_to_int16` / `sample_to_int8` UB on out-of-range samples |
| 5  | FIXED | Critical | Thread | Core | `Timer::thread_interval` uses `atomic_flag::test_and_set` incorrectly |
| 6  | FIXED | Critical | Thread | Core | `ReleasePool` destructor races with timer callback (use-after-free) |
| 7  | FIXED | Critical | Bug | Core | `Lib::initialize` swallows errors inside `call_once` |
| 8  | DEFERRED | Critical | Thread | Core | `Lib::terminate` not thread-safe; no re-initialization gate |
| 9  | FIXED | Critical | Bug | Backend | WASAPI: off-by-one heap buffer overflow in `construct` |
| 10 | ALREADY FIXED | Critical | Bug | Backend | WASAPI: `device->properties` assigns to wrong member |
| 11 | FIXED | Critical | Bug | Backend | WASAPI: `closest_match` COM memory leak |
| 12 | DEFERRED | Critical | Bug | Backend | CoreAudio: `stop()` ignores all errors and never uninitializes |
| 13 | DEFERRED | Critical | Bug | Backend | CoreAudio: `start()` leaks resources on partial failure |
| 14 | FIXED | Critical | Bug | Backend | CoreAudio: `get_device_name` returns `nullptr` for `std::string` (UB crash) |
| 15 | DEFERRED | Critical | Bug | C API | Entire C API is dead code and does not compile |
| 16 | DEFERRED | Critical | Thread | Source | `AudioMixer` stores raw `AudioSource*` with no lifetime guarantee |
| 17 | FIXED | High | Bug | Converter | `sample_to_int32` UB on out-of-range samples |
| 18 | FIXED | High | Bug | Source | `AudioVoice::seek_time` -- negative time causes UB in `static_cast<size_t>` |
| 19 | FIXED | High | Bug | Source | `AudioData::create_slice` -- negative seconds causes UB |
| 20 | FIXED | High | Bug | Source | `advance_id` wraps to 0 (invalid sentinel), permanently exhausting ID space |
| 21 | ALREADY FIXED | High | Bug | Source | `process_panning` -- `sqrt` of negative value produces NaN on bad input |
| 22 | FIXED | High | Semantic | Source | `AudioMixer::process_events` calls `on_removed_from_mixer` on never-added source |
| 23 | DEFERRED | High | Semantic | Source | Double volume/panning application in `AudioSpace::render` |
| 24 | DEFERRED | High | Thread | Source | `AudioVoice` compound state transitions observable in intermediate states |
| 25 | FIXED | High | Thread | Source | `AudioData::name` data race (no mutex unlike `AudioSource`) |
| 26 | DEFERRED | High | Bug | Reader | MP3 reader: VBR frame count may underestimate, silently losing frames |
| 27 | FIXED | High | Bug | Reader | Opus reader: potential buffer overrun if `op_pcm_total` underestimates |
| 28 | FIXED | High | Bug | Reader | Ogg reader: `assert(element_size == 1)` is no-op in release builds |
| 29 | DEFERRED | High | Bug | Converter | `sample_to_int24` returns unsigned-masked value in signed return type |
| 30 | DEFERRED | High | Bug | Converter | `write_sample` silently does nothing for `FLOAT_64` and `Unknown` formats |
| 31 | DEFERRED | High | Bug | Converter | ReSampler: `expected_frames` estimate may be too small; `int` overflow on large files |
| 32 | FIXED | High | Bug | Backend | WASAPI: `wc_to_utf8` leaks heap-allocated char array |
| 33 | FIXED | High | Bug | Backend | WASAPI: `device_id` allocated with `new` but never freed |
| 34 | FIXED | High | Bug | Backend | WASAPI: `cbSize` set to wrong value for `WAVEFORMATEXTENSIBLE` |
| 35 | FIXED | High | Bug | Backend | WASAPI: destructor does not stop audio thread before releasing resources |
| 36 | FIXED | High | Bug | Backend | WASAPI: no `CoInitializeEx` on audio callback thread |
| 37 | DEFERRED | High | Bug | Backend | CoreAudio: `create_device_properties` leaks test AudioUnit |
| 38 | DEFERRED | High | Bug | Backend | CoreAudio: `get_latency_*` / `set_frames_per_buffer` don't short-circuit on error |
| 39 | FIXED | High | Bug | Core | `File::read_buffer` truncates `size_t` to `long`; loses partial reads at EOF |
| 40 | FIXED | High | Bug | Core | Logger level filter only applied to built-in receiver, not custom receivers |
| 41 | DEFERRED | High | Bug | C API | Dangling pointer from `get_name()` returning `c_str()` of temporary |
| 42 | DEFERRED | High | Bug | C API | Virtual C++ structs exposed as C API -- not ABI-stable or C-compatible |
| 43 | DEFERRED | High | Bug | Demo | Off-by-one: `device_property_index > size()` should be `>=` |
| 44 | DEFERRED | High | Build | Build | `cmake_minimum_required(VERSION 3.31)` is too aggressive |
| 45 | FIXED | High | Build | Build | `test/CMakeLists.txt` typo: `CMAKE_CSS_STANDARD_LIBRARIES` |
| 46 | DEFERRED | Medium | Bug | Source | `AudioSpace` ID space: `uint16_t` exhaustion after 65534 allocations |
| 47 | DEFERRED | Medium | Thread | Source | `AudioSource::sample_rate` and `channel` are non-const, non-atomic |
| 48 | DEFERRED | Medium | Bug | Source | `AudioStream` ring buffer positions overflow on 32-bit after ~24 hours |
| 49 | ALREADY FIXED | Medium | Bug | Reader | WAV reader: unrecognized PCM bit depth leaves `sample_format` as `Unknown`, leaks `drwav` |
| 50 | FIXED | Medium | Bug | Reader | `create_audio_data`: `reinterpret_cast` from `uint8_t[]` violates alignment |
| 51 | FIXED | Medium | Bug | Reader | Opus reader: does not set error on `op_open_memory` failure |
| 52 | DEFERRED | Medium | Bug | Reader | MP3 / WAV: `DR_*_IMPLEMENTATION` defines risk ODR violations |
| 53 | FIXED | Medium | Bug | Converter | ReSampler: no null check on input `p_audio_data` |
| 54 | ALREADY FIXED | Medium | Bug | Converter | Channel converter: null `storage` pointer dereference when `frame_count == 0` |
| 55 | DEFERRED | Medium | Thread | Backend | CoreAudio: audio callback reads `audio_source` without memory fence |
| 56 | DEFERRED | Medium | Bug | Backend | CoreAudio: `property_callback` only processes first address in array |
| 57 | DEFERRED | Medium | Bug | Backend | CoreAudio: `create_description` returns zeroed struct for unsupported formats |
| 58 | DEFERRED | Medium | Bug | Backend | WASAPI: STA apartment model; audio thread may need MTA |
| 59 | DEFERRED | Medium | Bug | Backend | WASAPI: `start()` error paths leak `audio_client` and handles |
| 60 | DEFERRED | Medium | Bug | Backend | `AudioDevice::render_to_device_buffer` trusts caller buffer size |
| 61 | FIXED | Medium | Bug | Core | `Buffer::get_available()` underflows if `position > virtual_length` |
| 62 | FIXED | Medium | Bug | Core | `File::is_eof` returns `false` when no file is open |
| 63 | DEFERRED | Medium | Quality | Core | `_INLINE_` macro uses reserved identifier pattern |
| 64 | DEFERRED | Medium | Quality | Core | `#include <sal.h>` placed inside Logger class body |
| 65 | FIXED | Medium | Bug | Core | `Buffer::write_data` compares `size_t <= 0` (tautological) |
| 66 | OPEN | Medium | Build | Build | `CMAKE_OSX_ARCHITECTURES` set after `project()` -- may be too late |
| 67 | OPEN | Medium | Build | Build | `LOWL_DEBUG` defined as `PUBLIC`, leaking into consumers |
| 68 | OPEN | Medium | Architecture | Architecture | No Linux audio backend (PulseAudio / ALSA / PipeWire) |
| 69 | OPEN | Medium | Architecture | Architecture | `Lib::terminate` does not clear `drivers` or allow re-initialization |
| 70 | OPEN | Medium | Bug | Demo | `std::stoi` on user input with no exception handling |
| 71 | OPEN | Low | Quality | Source | `AudioSource` value-initializes atomics then re-stores in constructor |
| 72 | OPEN | Low | Quality | Source | `AudioBlockView::channel()` has no bounds check |
| 73 | OPEN | Low | Quality | Source | `AudioVoice` defaults to `Playing`; `AudioSpace` immediately stops it |
| 74 | OPEN | Low | Quality | Source | `get_frames_remaining()` returns 1 while `get_frame_count()` returns 0 |
| 75 | OPEN | Low | Quality | Reader | `detect_format` is extension-only; no magic-byte fallback |
| 76 | OPEN | Low | Quality | Reader | FLAC reader: `DR_FLAC_NO_CRC` disables integrity checks |
| 77 | OPEN | Low | Quality | Backend | `AudioDeviceProperties::operator<` inconsistent with fuzzy `operator==` |
| 78 | OPEN | Low | Quality | Backend | `AudioDriver::get_devices()` returns vector by value |
| 79 | OPEN | Low | Quality | Core | Include guards missing `_H` suffix across multiple headers |
| 80 | OPEN | Low | Quality | Core | `File` has undeclared `test()` method (dead declaration) |
| 81 | OPEN | Low | Build | Build | No compiler warning flags for GCC or MSVC |
| 82 | OPEN | Low | Quality | Demo | Global variables, inconsistent arg parsing, no `--help` |
| 83 | OPEN | Low | Testing | Testing | Multiple test coverage gaps (see section below) |

---

## Current Pass -- 2026-03-27

- **Issue 1 -- FIXED.** Options considered: add manual `drwav_uninit` calls on the current early returns, wrap `drwav` in a local RAII guard, or refactor WAV decode into a shared helper. Decision: manual cleanup plus explicit error propagation was the smallest architecture-fit patch for the existing reader.
- **Issue 2 -- FIXED.** Options considered: manual `drflac_close` on every branch, a `unique_ptr` with `drflac_close` deleter, or a broader FLAC reader refactor. Decision: the RAII deleter was the safest low-churn fix because it closed all paths without duplicating cleanup code.
- **Issue 3 -- FIXED.** Options considered: cast both operands to `uint64_t`, add a checked multiply helper, or clamp on `size_t` before multiplying. Decision: explicit `uint64_t` casts solved the concrete overflow without changing reader structure.
- **Issue 4 -- FIXED.** Options considered: clamp in each integer conversion, clamp once at call sites before conversion, or add a saturating helper used by all integer writers. Decision: local clamps in the conversion helpers matched the current design and removed the UB directly.
- **Issue 5 -- FIXED.** Options considered: replace `atomic_flag` with `atomic<bool>`, keep `atomic_flag` and restructure the loop, or replace the timer with a condition-variable based worker. Decision: `atomic<bool>` was the clearest low-risk correction and removed the re-arming bug.
- **Issue 6 -- FIXED.** Options considered: keep detach semantics and rely on caller discipline, make `stop()` always synchronize with thread exit, or redesign `ReleasePool` to avoid callbacks on `this`. Decision: removing the detach path from normal shutdown and making `stop()` wait from external threads fit the current `ReleasePool` usage best.
- **Issue 7 -- FIXED.** Options considered: capture the caller error into `call_once`, store initialization state in static error storage, or replace `call_once` entirely with a resettable state machine. Decision: static `initialization_error` storage fixed the swallowed-error behavior without taking on Issue 8’s larger lifecycle redesign.
- **Issue 8 -- DEFERRED.** Options considered: keep `once_flag` and document one-shot lifetime, replace it with a mutex-protected state machine, or split initialization/termination per backend. Decision: this needs a broader library-lifetime policy, especially around re-initialization and shared static driver state, so I did not patch it opportunistically.
- **Issue 9 -- FIXED.** Options considered: fix the off-by-one index, wrap the allocation in `std::wstring`, or remove the unused device-id copy entirely. Decision: removing the unused allocation was best because it eliminated both the overflow and the leak.
- **Issue 10 -- VERIFIED ALREADY FIXED.** Options considered: change the assignment, add a compatibility alias, or verify current code. Decision: current code already assigns to `properties_list`, so no source change was needed.
- **Issue 11 -- FIXED.** Options considered: free `closest_match` inline at each return, add a small local cleanup helper, or wrap the COM allocation. Decision: explicit `CoTaskMemFree` on all exit paths was the least invasive fix.
- **Issue 12 -- DEFERRED.** Options considered: minimally add error checks, fully uninitialize/remove listeners/callbacks, or rebuild CoreAudio device shutdown around RAII. Decision: the correct fix touches the whole CoreAudio lifecycle, so I left it for a dedicated pass.
- **Issue 13 -- DEFERRED.** Options considered: patch individual early returns, add scope guards around startup resources, or restructure `start()` into staged RAII objects. Decision: partial edits risked missing paths; this wants a full CoreAudio startup cleanup pass.
- **Issue 14 -- FIXED.** Options considered: return `""`, return `std::string()`, or propagate a separate failure object. Decision: returning an empty string is enough to remove the UB and preserve the current API.
- **Issue 15 -- DEFERRED.** Options considered: delete the C API, patch it enough to compile, or redesign it around opaque C handles. Decision: this is outside the current `src/` core-library pass and needs an intentional API decision first.
- **Issue 16 -- DEFERRED.** Options considered: convert mixer ownership to `shared_ptr`, make the raw-pointer contract explicit with acknowledgement requirements, or hide direct mixer usage behind `AudioSpace`. Decision: that is a public API/lifetime contract decision, not a safe opportunistic patch.
- **Issue 17 -- FIXED.** Options considered: clamp in `sample_to_int32`, clamp before every caller writes, or switch to a saturating helper. Decision: clamping inside `sample_to_int32` fixed the UB at the source.
- **Issue 18 -- FIXED.** Options considered: clamp negative seconds to zero, reject negative input, or switch to signed frame math first. Decision: clamping to zero matches the existing seek semantics and removes the undefined cast.
- **Issue 19 -- FIXED.** Options considered: clamp negative seconds to zero, reject negative slice bounds, or redesign the slice API around optional endpoints. Decision: clamping was the straightforward behavior-preserving fix.
- **Issue 20 -- FIXED.** Options considered: wrap IDs to `1`, reserve a separate exhaustion state, or widen the ID type immediately. Decision: wrapping to `1` is the minimal correct fix and matches generation handling.
- **Issue 21 -- VERIFIED ALREADY FIXED.** Options considered: add a clamp in `process_panning`, trust `set_panning`, or verify current code. Decision: current `process_panning` already clamps before `sqrt`, so no patch was needed.
- **Issue 22 -- FIXED.** Options considered: keep calling `on_removed_from_mixer`, call a new rejection callback, or send only the rejection acknowledgement. Decision: rejection-only behavior matched the actual state transition and avoided lying to the source.
- **Issue 23 -- DEFERRED.** Options considered: remove mixer gain, remove space gain, or document the hierarchy as intentional. Decision: this needs a product/API call about intended gain staging, so I did not change runtime behavior.
- **Issue 24 -- DEFERRED.** Options considered: tighten memory ordering, collapse state transitions into a single publish point, or redesign playback state exposure. Decision: that needs a deliberate concurrency model review rather than a narrow patch.
- **Issue 25 -- FIXED.** Options considered: add a mutex around `AudioData::name`, make names immutable, or move naming outside the audio object. Decision: matching `AudioSource` and guarding the string with a mutex was the most consistent low-risk fix.
- **Issue 26 -- DEFERRED.** Options considered: decode into a growable buffer, keep trimming and log truncation, or trust `drmp3_get_pcm_frame_count`. Decision: the correct behavior depends on whether silent truncation is acceptable for VBR input, so I left it open.
- **Issue 27 -- FIXED.** Options considered: clamp each decoded chunk to the remaining capacity, switch Opus decoding to a growable buffer, or trust `op_pcm_total`. Decision: chunk clamping removes the overflow risk without changing the current fixed-allocation approach.
- **Issue 28 -- FIXED.** Options considered: keep the assert, compute byte counts and return item counts correctly, or refuse non-1-byte reads outright. Decision: implementing correct item-sized reads made the callback valid in both debug and release builds.
- **Issue 29 -- DEFERRED.** Options considered: keep the packed `int32_t` contract, change the return type to `uint32_t`, or add a separate sign-extended helper. Decision: the existing write path may rely on packed low-24-bit behavior, so this needs a contract decision.
- **Issue 30 -- DEFERRED.** Options considered: implement `FLOAT_64`, assert/fail on unsupported formats, or add an error-returning write API. Decision: the current helper has no error channel, so fixing `Unknown` cleanly needs an API decision rather than a partial patch.
- **Issue 31 -- DEFERRED.** Options considered: over-allocate output with margin, chunk the resampling work, or add hard guards around `int` conversion. Decision: this depends on the exact r8b output contract and deserves a focused resampler pass.
- **Issue 32 -- FIXED.** Options considered: manually `delete[]` the UTF-8 buffer, wrap it in smart ownership, or return `std::string`. Decision: returning `std::string` matched the rest of the code and removed ownership ambiguity entirely.
- **Issue 33 -- FIXED.** Options considered: free the copied device id, store it in an owning C++ type, or remove it because it was unused. Decision: removing the unused copy was the cleanest result.
- **Issue 34 -- FIXED.** Options considered: leave `cbSize` as-is, set it to the documented extensible payload size, or special-case per format. Decision: the documented `sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX)` value was the right direct fix.
- **Issue 35 -- FIXED.** Options considered: trust callers to stop before destruction, stop in the destructor, or move thread ownership out of the device. Decision: calling `stop()` in the destructor is the minimal correct safety fix.
- **Issue 36 -- FIXED.** Options considered: initialize COM on the callback thread, rely on the creating thread’s COM state, or move callback work behind a COM-aware wrapper. Decision: explicit callback-thread `CoInitializeEx` was the correct per-thread fix.
- **Issue 37 -- DEFERRED.** Options considered: add local disposal calls, wrap test units in RAII, or redesign CoreAudio probing. Decision: this belongs with the broader CoreAudio resource-management pass.
- **Issue 38 -- DEFERRED.** Options considered: add immediate `error.has_error()` short-circuits, change the helpers to return richer result types, or restructure CoreAudio utility composition. Decision: this is best handled together with the other CoreAudio cleanup work.
- **Issue 39 -- FIXED.** Options considered: cast to `std::streamsize`, chunk large reads manually, or leave the API and only fix EOF handling. Decision: switching to `std::streamsize` and preserving `gcount()` on EOF solved the concrete bug without redesigning file I/O.
- **Issue 40 -- FIXED.** Options considered: keep filtering only in the stdout receiver, push the filter into `Logger::write`, or require custom receivers to filter themselves. Decision: central filtering in `Logger::write` gives consistent behavior for all receivers.
- **Issue 41 -- DEFERRED.** Options considered: store stable C strings, rewrite around opaque handles, or remove the C API. Decision: this is part of the larger C API rewrite/removal decision and stayed out of the current pass.
- **Issue 42 -- DEFERRED.** Options considered: patch ABI details piecemeal, redesign as a real C API, or remove it. Decision: only a full redesign/removal is credible here, so I did not make partial edits.
- **Issue 43 -- DEFERRED.** Options considered: fix the demo bounds check, remove the demo code, or leave demos out of the core pass. Decision: demo code is outside the current repository-local scope for this pass.
- **Issue 44 -- DEFERRED.** Options considered: lower the root CMake version, split minimum versions per subtree, or leave build requirements unchanged. Decision: root build files were outside this pass’s scope.
- **Issue 45 -- FIXED.** Options considered: leave the typo, correct the variable in place, or restructure MinGW link flags entirely. Decision: correcting the typo was safe and directly improved the test build.
- **Issue 46 -- DEFERRED.** Options considered: widen IDs to `uint32_t`, keep `uint16_t` and rely on free-list reuse, or add explicit exhaustion handling. Decision: widening public handle types is a broader ABI/API choice.
- **Issue 47 -- DEFERRED.** Options considered: make fields `const`, make them atomic, or document publication ordering. Decision: this is low-risk cleanup, but it touches type declarations and publication guarantees beyond the current bug-fix pass.
- **Issue 48 -- DEFERRED.** Options considered: re-base positions periodically, require 64-bit `size_t`, or make the ring indices fixed-width 64-bit counters. Decision: that needs a dedicated 32-bit correctness pass.
- **Issue 49 -- VERIFIED ALREADY FIXED.** Options considered: add an explicit `Unknown` fast-fail before `create_audio_data`, rely on post-call `drwav_uninit`, or wrap `drwav` in RAII. Decision: after the current cleanup pass, `drwav_uninit` still runs on this path, so no extra code was required.
- **Issue 50 -- FIXED.** Options considered: trust allocator alignment, copy into aligned typed buffers first, or replace typed buffer access with `memcpy`. Decision: `memcpy` inside the conversion path removed the UB without changing the external reader API.
- **Issue 51 -- FIXED.** Options considered: map all Opus errors to one generic code, preserve the vendor error code, or leave the null return unannotated. Decision: preserving the vendor error code gives callers the most useful signal with minimal churn.
- **Issue 52 -- DEFERRED.** Options considered: leave implementation defines in place, move each implementation define into a dedicated TU, or build wrapper libraries. Decision: that is a build-structure change and not a quick library-core patch.
- **Issue 53 -- FIXED.** Options considered: assume non-null input, add a null early return, or surface an error object from the resampler. Decision: early return was consistent with the current signature and removed the null dereference.
- **Issue 54 -- VERIFIED ALREADY FIXED.** Options considered: add a zero-frame early return, restructure pointer arithmetic, or verify the current guards. Decision: the existing `storage ? ... : nullptr` path already avoids null-pointer arithmetic here.
- **Issue 55 -- DEFERRED.** Options considered: make callback-visible fields atomic, add explicit synchronization at startup, or document CoreAudio’s publication guarantees. Decision: this belongs in the CoreAudio concurrency pass.
- **Issue 56 -- DEFERRED.** Options considered: iterate all addresses, split callback handling by selector, or leave current single-address behavior. Decision: not enough CoreAudio callback coverage was in scope for this pass.
- **Issue 57 -- DEFERRED.** Options considered: signal failure with `Error`, return `std::optional`, or keep zeroed descriptions. Decision: fixing this cleanly needs a small CoreAudio API redesign.
- **Issue 58 -- DEFERRED.** Options considered: switch global WASAPI COM init to MTA, keep STA and rely on thread-local COM init, or document the split model. Decision: Issue 36 is fixed, but the library-wide apartment policy still needs a deliberate backend decision.
- **Issue 59 -- DEFERRED.** Options considered: add manual cleanup at each error site, use scope guards, or refactor `start()` into staged helpers. Decision: this wants a focused WASAPI startup cleanup pass.
- **Issue 60 -- DEFERRED.** Options considered: trust the API contract, add debug assertions on written byte count, or change the API to pass an explicit output span. Decision: strict runtime enforcement needs API-level bounds information that the current signature does not carry.
- **Issue 61 -- FIXED.** Options considered: clamp the subtraction, assert `position <= virtual_length`, or rely on callers. Decision: clamping to zero was the safest behavior-preserving fix.
- **Issue 62 -- FIXED.** Options considered: return `true` when unopened, add `is_open()`, or leave the semantic mismatch alone. Decision: returning `true` matches the existing empty-read behavior without expanding the API.
- **Issue 63 -- DEFERRED.** Options considered: rename `_INLINE_`, leave it alone, or replace it with compiler attributes directly. Decision: this is worthwhile cleanup, but orthogonal to the correctness fixes in this pass.
- **Issue 64 -- DEFERRED.** Options considered: move the include, leave the current conditional layout, or restructure the logger header. Decision: header-hygiene cleanup was lower priority than the concrete functional bugs fixed here.
- **Issue 65 -- FIXED.** Options considered: leave the unsigned comparison, change only the reported line, or normalize the checks to `== 0`. Decision: `== 0` is the correct low-noise fix and removes the tautological compare.

---

## Issue 1 -- WAV Reader Leaks `drwav` on Error Paths -- FIXED

**Severity:** Critical
**Category:** Bug (Resource Leak)
**Files:** `src/audio/reader/lowl_audio_reader_wav.cpp:19-29`

### Problem

After `drwav_init_memory` succeeds (line 13), two early-return paths (lines 22-23 and 28-29) return `nullptr` without calling `drwav_uninit(&wav)`. The code even has a `// todo uninit?` comment on line 21 acknowledging the problem. The `drwav_uninit` call only happens on line 110, well past these exits. Every WAV file that hits these paths leaks the decoder's internal state. Additionally, these error paths do not set the `error` parameter, so callers get a null result with no error indication.

### Fix

Call `drwav_uninit(&wav)` and set an appropriate error code before each `return nullptr`.

---

## Issue 2 -- FLAC Reader Never Calls `drflac_close` -- FIXED

**Severity:** Critical
**Category:** Bug (Resource Leak)
**Files:** `src/audio/reader/lowl_audio_reader_flac.cpp:13-50`

### Problem

`drflac_open_memory` returns a heap-allocated `drflac*` on line 13. This pointer is never freed. `drflac_close(flac)` must be called to release it. None of the code paths (success or error) call it. Every FLAC file loaded leaks the entire decoder state.

### Fix

Call `drflac_close(flac)` after `drflac_read_pcm_frames_s32` returns, on all paths including the `bytes_to_read_test == 0` early exit.

---

## Issue 3 -- Integer Overflow in Reader Size Calculations on 32-bit -- FIXED

**Severity:** Critical
**Category:** Bug (Integer Overflow)
**Files:** `src/audio/reader/lowl_audio_reader_flac.cpp:28`, `src/audio/reader/lowl_audio_reader_wav.cpp:33`

### Problem

`flac->totalPCMFrameCount * bytes_per_frame` where `bytes_per_frame` is `size_t`. On 32-bit builds, `size_t` is 32 bits, and the multiplication can silently overflow before being assigned to `uint64_t`. The same applies to the WAV reader. A large audio file (>512MB of PCM data) triggers the overflow, causing a too-small allocation followed by buffer overwrite.

### Fix

Explicitly cast both operands to `uint64_t` before multiplying: `static_cast<uint64_t>(frame_count) * static_cast<uint64_t>(bytes_per_frame)`.

---

## Issue 4 -- `sample_to_int16` / `sample_to_int8` UB on Out-of-Range Samples -- FIXED

**Severity:** Critical
**Category:** Bug (Undefined Behavior)
**Files:** `src/audio/convert/lowl_audio_sample_converter.h:60-61, 73-75`

### Problem

`sample_to_int16` computes `static_cast<int16_t>(p_sample * 32767.0f)`. If `p_sample > 1.0f` or `p_sample < -1.0f` (possible from resampling, mixing, or accumulated float error), the result exceeds the `int16_t` range. Casting a float outside the destination integer's range is undefined behavior in C++. `sample_to_int8` has the same issue. Contrast with `sample_to_uint8` which correctly calls `std::clamp` first.

### Fix

Add `std::clamp(p_sample, -1.0f, 1.0f)` before the multiply, matching `sample_to_uint8`.

---

## Issue 5 -- `Timer::thread_interval` Uses `atomic_flag::test_and_set` Incorrectly -- FIXED

**Severity:** Critical
**Category:** Bug / Thread Safety
**Files:** `src/lowl_timer.h:20-26`

### Problem

`thread_interval` calls `running.test_and_set()` on line 21 to "arm" the flag, then loops with `while (running.test_and_set())`. `test_and_set` always sets the flag and returns the old value. If `stop()` is called from the timer callback itself, `running.clear()` is called, but after the callback returns, the loop calls `test_and_set()` again, re-arms the flag, and continues running. The `detach()` fallback path in `stop()` means the thread keeps running after the Timer or ReleasePool object is destroyed -- use-after-free.

### Fix

Replace `std::atomic_flag` with `std::atomic<bool>`. Check the flag without modifying it: `while (running.load()) { ... }`.

---

## Issue 6 -- `ReleasePool` Destructor Races with Timer Callback -- FIXED

**Severity:** Critical
**Category:** Thread Safety / Use-After-Free
**Files:** `src/lowl_release_pool.h:43-51`

### Problem

The destructor calls `timer->stop()`, but `release_callback` captures `this` via `std::bind`. If the timer's `stop()` detaches the thread (which happens if stop is called from the callback thread), the callback may still run after `~ReleasePool()` completes, accessing the destroyed `mutex` and `pool`. Even with the existing `timer->stop()` in the destructor, the `Timer::stop()` implementation has the `detach()` path that doesn't guarantee the thread has finished.

### Fix

Ensure the timer thread is always joined (never detached) before the destructor returns. The `Timer` class needs a clean shutdown that guarantees the callback has finished.

---

## Issue 7 -- `Lib::initialize` Swallows Errors Inside `call_once` -- FIXED

**Severity:** Critical
**Category:** Bug
**Files:** `src/lowl.cpp:25-41`

### Problem

`initialize` accepts `Error &error` but the `call_once` lambda cannot propagate errors back through that parameter on subsequent calls. If WASAPI COM initialization fails, the local error is silently discarded. Every subsequent call to `initialize` is a no-op (the once_flag is consumed), so the caller always sees `NoError` even though initialization actually failed.

### Fix

Capture `&error` in the lambda, or store the init error in a static member and return it from `get_drivers()` / `get_default_device()`.

---

## Issue 8 -- `Lib::terminate` Not Thread-Safe; No Re-Initialization Gate -- OPEN

**Severity:** Critical
**Category:** Thread Safety
**Files:** `src/lowl.cpp:43-47`

### Problem

`terminate()` calls into WASAPI COM teardown with no synchronization. After `terminate()`, `initialized` (once_flag) is already consumed, so calling `initialize()` again is a no-op -- the drivers vector still holds stale pointers to a torn-down subsystem. There is no synchronization on the `drivers` static vector between `terminate()` and concurrent reads.

### Fix

Clear the `drivers` vector in `terminate()`, add a mutex for the static state, and use a resettable mechanism instead of `std::once_flag`.

---

## Issue 9 -- WASAPI: Off-by-One Heap Buffer Overflow in `construct` -- FIXED

**Severity:** Critical
**Category:** Bug
**Files:** `src/audio/backend/wasapi/lowl_audio_wasapi_device.cpp:514`

### Problem

`device_id[device_id_len + 1] = '\0'` writes one past the allocated buffer. The buffer is `new WCHAR[device_id_len + 1]`, so valid indices are `0` through `device_id_len`. The null terminator should be at `device_id_len`, not `device_id_len + 1`.

### Fix

`device_id[device_id_len] = '\0';`

---

## Issue 10 -- WASAPI: `device->properties` Assigns to Wrong Member -- ALREADY FIXED

**Severity:** Critical
**Category:** Bug
**Files:** `src/audio/backend/wasapi/lowl_audio_wasapi_device.cpp:565`

### Problem

`device->properties = audio_device_properties;` assigns to a member named `properties`, but the base class `AudioDevice` declares the member as `properties_list`. This either fails to compile, or (if there is a shadow member) silently populates the wrong field, meaning `get_properties_list()` and `get_closest_properties()` return empty for all WASAPI devices.

### Fix

Change to `device->properties_list = audio_device_properties;`

---

## Issue 11 -- WASAPI: `closest_match` COM Memory Leak -- FIXED

**Severity:** Critical
**Category:** Bug (Resource Leak)
**Files:** `src/audio/backend/wasapi/lowl_audio_wasapi_device.cpp:877-897`

### Problem

When `IsFormatSupported` returns `S_FALSE`, it allocates and returns a `WAVEFORMATEX*` via `closest_match`. This pointer is never freed with `CoTaskMemFree`. Every shared-mode property validation leaks COM memory.

### Fix

Add `CoTaskMemFree(closest_match);` after using it.

---

## Issue 12 -- CoreAudio: `stop()` Ignores All Errors and Never Uninitializes -- OPEN

**Severity:** Critical
**Category:** Bug
**Files:** `src/audio/backend/coreaudio/lowl_audio_core_audio_device.cpp:296-301`

### Problem

`stop()` calls `AudioOutputUnitStop` and `AudioUnitReset`, assigning both results to the same variable (overwriting the first) and checking neither. The AudioUnit is never uninitialized (`AudioUnitUninitialize`), property listeners are never removed, and the render callback is never unregistered. The device is left in a partially-torn-down state.

### Fix

Check return values, call `AudioUnitUninitialize`, remove listeners and callback, and report errors via the `error` parameter.

---

## Issue 13 -- CoreAudio: `start()` Leaks Resources on Partial Failure -- OPEN

**Severity:** Critical
**Category:** Bug (Resource Leak)
**Files:** `src/audio/backend/coreaudio/lowl_audio_core_audio_device.cpp:158-293`

### Problem

If `start()` fails midway (after creating the AudioUnit but before starting it, or after adding listeners), the already-created AudioUnit, registered property listeners, and allocated render buffer are leaked. A subsequent `start()` call would leak the old AudioUnit.

### Fix

Add cleanup logic (dispose audio unit, remove listeners) on each early return path, or use RAII wrappers.

---

## Issue 14 -- CoreAudio: `get_device_name` Returns `nullptr` for `std::string` -- FIXED

**Severity:** Critical
**Category:** Bug (UB / Crash)
**Files:** `src/audio/backend/coreaudio/lowl_audio_core_audio_utilities.cpp:18`

### Problem

`return nullptr;` in a function returning `std::string` constructs a `std::string` from `nullptr`, which is undefined behavior per the C++ standard. On most implementations this crashes with a null dereference inside the string constructor.

### Fix

Return `std::string()` or `""`.

---

## Issue 15 -- C API Is Dead Code and Does Not Compile -- OPEN

**Severity:** Critical
**Category:** Bug / Architecture
**Files:** `c_api/lowl_interface.h`, `c_api/lowl_interface.cpp`

### Problem

Both files are wrapped in `#ifdef LOWL_LIBRARY`, which is never defined in the build system. The `c_api/` directory is never added via `add_subdirectory()`. Even if enabled, the code references types (`LowlError`, `LowlDriver`, `Lowl`) that do not exist in the codebase -- the actual types are `Lowl::Error`, `Lowl::Audio::AudioDriver`, `Lowl::Lib`. The API also uses virtual C++ structs (not C-compatible), Windows-only export macros, and `get_name()` returns `c_str()` of a temporary string (dangling pointer). The C API is nonfunctional and needs a complete rewrite if C interop is a goal.

### Fix

Either remove the C API entirely, or rewrite using opaque handles and free functions with platform-portable export macros.

---

## Issue 16 -- `AudioMixer` Stores Raw `AudioSource*` with No Lifetime Guarantee -- OPEN

**Severity:** Critical
**Category:** Bug / Architecture
**Files:** `src/audio/source/lowl_audio_mixer.h:28`, `src/audio/source/lowl_audio_mixer_event.h:19`

### Problem

The mixer stores raw `AudioSource*` pointers. Nothing prevents the owner from destroying the source while it is still in the mixer's active array. Between enqueuing a `Remove` event and the audio thread processing it in `process_events()`, the source is still active. If the caller deletes the source after enqueueing `Remove`, the audio thread dereferences a dangling pointer in `render_mixed_block()`. The `AudioSpace` wrapper mitigates this through the ack/generation mechanism, but the `AudioMixer` public API is unsafe for direct use.

### Fix

Either use `std::shared_ptr` in the mixer's active array, or require callers to wait for an acknowledgement before destroying the source. Document the contract explicitly.

---

## Issue 17 -- `sample_to_int32` UB on Out-of-Range Samples -- FIXED

**Severity:** High
**Category:** Bug (Undefined Behavior)
**Files:** `src/audio/convert/lowl_audio_sample_converter.h:55-58`

### Problem

`p_sample * 0x7FFFFFFF` where `0x7FFFFFFF = INT32_MAX`. If `p_sample` is even slightly above `1.0` (e.g., `1.0000001`), the product exceeds `INT32_MAX` and the `static_cast<int32_t>` is undefined behavior. No clamping is performed.

### Fix

Clamp `p_sample` to `[-1.0, 1.0]` before the multiply.

---

## Issue 18 -- `AudioVoice::seek_time` -- Negative Time Causes UB -- FIXED

**Severity:** High
**Category:** Bug
**Files:** `src/audio/source/lowl_audio_voice.cpp:115`

### Problem

`seek_time` casts `p_seconds * sample_rate` to `size_t`. If `p_seconds` is negative, the multiplication produces a negative double, and `static_cast<size_t>` on a negative double is undefined behavior.

### Fix

Clamp `p_seconds` to `>= 0.0` before the cast.

---

## Issue 19 -- `AudioData::create_slice` -- Negative Seconds Causes UB -- FIXED

**Severity:** High
**Category:** Bug
**Files:** `src/audio/source/lowl_audio_data.cpp:28-29`

### Problem

`static_cast<size_t>(p_begin_sec * sample_rate)` is UB when `p_begin_sec` is negative. The parameter type is `double`, so negative values are representable.

### Fix

Clamp both `p_begin_sec` and `p_end_sec` to `>= 0.0` before the cast.

---

## Issue 20 -- `advance_id` Wraps to 0, Permanently Exhausting ID Space -- FIXED

**Severity:** High
**Category:** Bug
**Files:** `src/audio/source/lowl_audio_space.cpp:14-18`

### Problem

`advance_id` returns 0 when `p_current == max`. For `AudioAssetId` (uint16_t), max is 65535, so the next ID is 0 (`InvalidAudioAssetId`). The caller checks for this and returns an error. But `advance_generation` correctly wraps to 1 (skipping 0). The inconsistency means once the ID counter reaches max, `current_audio_asset_id` stays at 0 permanently -- no new assets or playbacks can be created even if the free-list has slots.

### Fix

Change `advance_id` to wrap to 1 (or `FirstAudioAssetId` / `FirstPlaybackSlotId`) instead of 0, matching `advance_generation`'s skip-zero pattern.

---

## Issue 21 -- `process_panning` -- `sqrt` of Negative Produces NaN -- ALREADY FIXED

**Severity:** High
**Category:** Bug
**Files:** `src/audio/source/lowl_audio_source.cpp:78, 88`

### Problem

If `panning` is set outside `[-1, 1]` (via a bug or bypass of `set_panning`), then `std::sqrt(1.0 - pan)` or `std::sqrt(1.0 + pan)` receives a negative argument, producing NaN. NaN propagation corrupts the output buffer and all downstream audio.

### Fix

Add a clamp inside `process_panning`: `pan = std::clamp(pan, -1.0f, 1.0f)`.

---

## Issue 22 -- Mixer Calls `on_removed_from_mixer` on Never-Added Source -- FIXED

**Severity:** High
**Category:** Semantic
**Files:** `src/audio/source/lowl_audio_mixer.cpp:139`

### Problem

When the mixer is full (`find_free_source_index` returns invalid), the code calls `event.audio_source->on_removed_from_mixer()` even though the source was never added. For `AudioVoice`, this sets `detached = true`, which is misleading.

### Fix

Do not call `on_removed_from_mixer()` when the source was never added. Only send the `Rejected` ack.

---

## Issue 23 -- Double Volume/Panning Application in `AudioSpace::render` -- OPEN

**Severity:** High
**Category:** Semantic
**Files:** `src/audio/source/lowl_audio_space.cpp:474-486`

### Problem

`AudioSpace::render` calls `mixer->render(p_block)`. The mixer's `render_mixed_block` already calls `process_volume` and `process_panning` (the mixer is itself an `AudioSource`). Then `AudioSpace::render` applies `process_volume` and `process_panning` *again*. This means output gets: `voice_volume * mixer_volume * space_volume`. If this hierarchical gain staging is intentional, it is undocumented. If not, volume is applied twice.

### Fix

If intentional, document the gain hierarchy. If not, remove the mixer's own volume/panning processing or the space's.

---

## Issue 24 -- `AudioVoice` Compound State Transitions Observable in Intermediate States -- OPEN

**Severity:** High
**Category:** Thread Safety
**Files:** `src/audio/source/lowl_audio_voice.cpp:139-161`

### Problem

`restart_playback()` calls `reset()` then `play()` then stores to `playback_state` -- three separate relaxed atomic operations with no fence between them. The render thread could observe `playback_enabled == true` (from `play()`) before the seek position of 0 has propagated, causing the voice to render from a stale position for one buffer. Similarly `stop_playback()` calls `pause()` then stores `Stopped` then `reset()` with no compound atomicity.

### Fix

Use `memory_order_release` on the final store in compound transitions, or collapse multi-step logic so the render thread observes a consistent state.

---

## Issue 25 -- `AudioData::name` Data Race -- FIXED

**Severity:** High
**Category:** Thread Safety
**Files:** `src/audio/source/lowl_audio_data.cpp:93-98`

### Problem

`AudioData::get_name()` and `set_name()` access `name` (a `std::string`) without synchronization. Unlike `AudioSource` which protects `name` with `name_mutex`, `AudioData` has none. If `set_name` is called from the main thread while the render thread reads the name (e.g., via logging), this is a data race.

### Fix

Add a mutex, or make `name` immutable (set only in constructor / factory).

---

## Issue 26 -- MP3 Reader: VBR Frame Count May Underestimate -- OPEN

**Severity:** High
**Category:** Bug
**Files:** `src/audio/reader/lowl_audio_reader_mp3.cpp:48-72`

### Problem

`drmp3_get_pcm_frame_count` returns an estimate for VBR MP3 files and may undercount. Storage is allocated based on `total_frames`. If the decoder produces fewer frames, the loop exits early and audio is silently truncated with no warning.

### Fix

Decode into a dynamically growing buffer, or log a warning on early loop exit.

---

## Issue 27 -- Opus Reader: Potential Buffer Overrun -- FIXED

**Severity:** High
**Category:** Bug
**Files:** `src/audio/reader/lowl_audio_reader_opus.cpp:34-52`

### Problem

Storage is allocated for `frame_count` frames (from `op_pcm_total`). The decode loop checks `frames_read_total < frame_count` at the top but does not check whether newly decoded data fits. If `frames_read_total + frames_read > frame_count`, the write to `storage.get() + channel_index * frame_count + frames_read_total` overflows.

### Fix

After `op_read_float`, clamp: `frames_read = std::min(frames_read, frame_count - frames_read_total)`.

---

## Issue 28 -- Ogg Reader: `assert(element_size == 1)` No-Op in Release -- FIXED

**Severity:** High
**Category:** Bug
**Files:** `src/audio/reader/lowl_audio_reader_ogg.cpp:18`

### Problem

In release builds (`NDEBUG` defined), `assert` is a no-op. If vorbisfile calls with `element_size != 1`, the function reads the wrong number of bytes (`element_count` instead of `element_size * element_count`).

### Fix

Compute `size_t total = element_size * element_count` and use that as the read limit.

---

## Issue 29 -- `sample_to_int24` Returns Unsigned-Masked Value in Signed Return Type -- OPEN

**Severity:** High
**Category:** Bug
**Files:** `src/audio/convert/lowl_audio_sample_converter.h:51-53`

### Problem

`& 0xFFFFFF` masks the result to 24 bits, zeroing the upper 8 bits. For negative values, this produces a value in range `[0, 16777215]` (always non-negative) even though the return type is `int32_t` and the function name suggests a signed 24-bit value. The `write_sample` path works correctly because it only reads the low 3 bytes, but any caller expecting a sign-extended int32 will get wrong values.

### Fix

Document that the return is an unsigned 24-bit value packed in `int32_t`, or change return type to `uint32_t`.

---

## Issue 30 -- `write_sample` Silently Does Nothing for FLOAT_64 and Unknown -- OPEN

**Severity:** High
**Category:** Bug
**Files:** `src/audio/convert/lowl_audio_sample_converter.h:119-122`

### Problem

The `SampleFormat::Unknown` and `SampleFormat::FLOAT_64` cases have empty bodies -- they don't advance the destination pointer or report an error. A caller writing FLOAT_64 samples gets no output, and the pointer never advances, potentially causing an infinite loop.

### Fix

Implement FLOAT_64 write support, or assert/error for unsupported formats.

---

## Issue 31 -- ReSampler: Output Size Estimate and `int` Overflow -- OPEN

**Severity:** High
**Category:** Bug
**Files:** `src/audio/convert/lowl_audio_re_sampler_r8b.cpp:8-9, 26`

### Problem

Two issues: (1) `expected_frames` estimate may be smaller than what r8b actually produces (due to filter latency), causing out-of-bounds writes. (2) `static_cast<int>(total_frames)` on line 26 truncates `size_t` to `int`. For a 48kHz 12-hour recording (~2 billion frames), this overflows.

### Fix

Add margin to `expected_frames` (+32), check r8b's return value for actual frames produced. Validate that `total_frames` fits in `int` or process in chunks.

---

## Issue 32 -- WASAPI: `wc_to_utf8` Leaks Memory -- FIXED

**Severity:** High
**Category:** Bug (Resource Leak)
**Files:** `src/audio/backend/wasapi/lowl_audio_wasapi_device.cpp:570-576`

### Problem

`wc_to_utf8` allocates a `char[]` with `new` and returns the raw pointer. The caller stores it in `const char *device_name_ptr` but never calls `delete[]`.

### Fix

Return `std::string` directly from `wc_to_utf8`.

---

## Issue 33 -- WASAPI: `device_id` Allocated but Never Freed -- FIXED

**Severity:** High
**Category:** Bug (Resource Leak)
**Files:** `src/audio/backend/wasapi/lowl_audio_wasapi_device.cpp:512-515`

### Problem

`new WCHAR[device_id_len + 1]` is allocated but neither stored in the device nor freed.

### Fix

Use `std::wstring` or `std::unique_ptr<WCHAR[]>`, or remove the allocation if unused.

---

## Issue 34 -- WASAPI: `cbSize` Set to Wrong Value -- FIXED

**Severity:** High
**Category:** Bug
**Files:** `src/audio/backend/wasapi/lowl_audio_wasapi_device.cpp:806`

### Problem

`wfe.Format.cbSize = sizeof(wfe)` sets it to `sizeof(WAVEFORMATEXTENSIBLE)` (~40 bytes). Per Windows docs, it should be `sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX)` (~22 bytes). An incorrect `cbSize` causes `IAudioClient::Initialize` to fail on some drivers.

### Fix

`wfe.Format.cbSize = sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX);`

---

## Issue 35 -- WASAPI: Destructor Does Not Stop Audio Thread -- FIXED

**Severity:** High
**Category:** Thread Safety
**Files:** `src/audio/backend/wasapi/lowl_audio_wasapi_device.cpp:956-963`

### Problem

The destructor releases COM interfaces and closes handles without stopping the audio thread first. If the thread is running, it will access freed interfaces -- use-after-free.

### Fix

Call `stop()` in the destructor before releasing resources.

---

## Issue 36 -- WASAPI: No COM Initialization on Audio Callback Thread -- FIXED

**Severity:** High
**Category:** Bug
**Files:** `src/audio/backend/wasapi/lowl_audio_wasapi_device.cpp:404-500`

### Problem

The audio callback thread (created via `CreateThread`) calls WASAPI COM methods (`GetBufferSize`, `GetCurrentPadding`, `GetBuffer`, `ReleaseBuffer`) without calling `CoInitializeEx`. COM must be initialized per-thread.

### Fix

Call `CoInitializeEx(nullptr, COINIT_MULTITHREADED)` at the start of `audio_callback()`.

---

## Issue 37 -- CoreAudio: `create_device_properties` Leaks Test AudioUnit -- OPEN

**Severity:** High
**Category:** Bug (Resource Leak)
**Files:** `src/audio/backend/coreaudio/lowl_audio_core_audio_device.cpp:314-399`

### Problem

`create_audio_unit` allocates an AudioUnit via `AudioComponentInstanceNew`. The `test_audio_unit` is used for testing properties but never disposed with `AudioComponentInstanceDispose`. One leak per device per initialization.

### Fix

Add `AudioComponentInstanceDispose(test_audio_unit)` before return on all paths.

---

## Issue 38 -- CoreAudio: Utility Functions Don't Short-Circuit on Error -- OPEN

**Severity:** High
**Category:** Bug
**Files:** `src/audio/backend/coreaudio/lowl_audio_core_audio_utilities.cpp:233-258, 448-473`

### Problem

`get_latency_high`, `get_latency_low`, and `set_frames_per_buffer` call multiple CoreAudio APIs in sequence without checking `error.has_error()` between calls. If an early call fails, later calls use garbage values and may overwrite the first error.

### Fix

Check `error.has_error()` after each sub-call and return early on failure.

---

## Issue 39 -- `File::read_buffer` Truncates `size_t` to `long`; Loses Partial Reads -- FIXED

**Severity:** High
**Category:** Bug
**Files:** `src/lowl_file.cpp:70-91`

### Problem

Two issues: (1) `static_cast<long>(length)` truncates `size_t` to `long`. On Windows LLP64, this truncates files >2GB. The parameter should be `std::streamsize`. (2) On a short read at EOF, both `eof()` and `fail()` are set, so the fail check overwrites `length` to 0, losing the partial data that was successfully read.

### Fix

Cast to `std::streamsize`. Check `fail()` only when `eof()` is false, or use `gcount()` directly.

---

## Issue 40 -- Logger Level Filter Only Applied to Built-in Receiver -- FIXED

**Severity:** High
**Category:** Bug / Semantic
**Files:** `src/lowl_logger.cpp:37, 44-49`

### Problem

`Logger::write()` dispatches to the receiver unconditionally. Only `std_out_log_receiver` checks `log_level`. Any custom receiver registered via `register_log_receiver` receives all messages regardless of the configured log level.

### Fix

Apply the level filter in `Logger::write()` before dispatching to the receiver.

---

## Issue 41 -- C API: Dangling Pointer from `get_name()` -- OPEN

**Severity:** High
**Category:** Bug
**Files:** `c_api/lowl_interface.cpp:16`

### Problem

`get_name()` returns `driver->get_name().c_str()` where `get_name()` returns `std::string` by value. The `c_str()` pointer is invalidated when the temporary is destroyed at end of statement.

### Fix

Store the name as a member and return a pointer to the stored copy.

---

## Issue 42 -- C API: Virtual C++ Structs Exposed as C API -- OPEN

**Severity:** High
**Category:** API Design / Architecture
**Files:** `c_api/lowl_interface.h:15-38`

### Problem

The C API uses `struct` with `public:` and `virtual` methods -- these are C++ polymorphic types, not C-compatible. The vtable layout is compiler-specific and not ABI-stable. Additionally, Windows-only export macros (`__stdcall`, `__declspec`) are used with no platform detection.

### Fix

Use opaque handles and free functions for C interop. Add platform-portable export macros.

---

## Issue 43 -- Demo: Off-by-One in Bounds Check -- OPEN

**Severity:** High
**Category:** Bug
**Files:** `demo/main.cpp:185`

### Problem

`if (device_property_index > device_properties_list.size())` should be `>=`. When `device_property_index == size()`, the check passes and `device_properties_list[device_property_index]` is an out-of-bounds access.

### Fix

Change `>` to `>=`.

---

## Issue 44 -- CMake Minimum Version Too Aggressive -- OPEN

**Severity:** High
**Category:** Build
**Files:** `CMakeLists.txt:1`, `third_party/CMakeLists.txt:1`

### Problem

`cmake_minimum_required(VERSION 3.31)` was released late 2024. Most CI systems and developer machines have older versions. Nothing in the build requires features beyond CMake 3.14-3.16.

### Fix

Lower to `cmake_minimum_required(VERSION 3.16)`.

---

## Issue 45 -- Test CMakeLists.txt Typo: `CMAKE_CSS_STANDARD_LIBRARIES` -- FIXED

**Severity:** High
**Category:** Build
**Files:** `test/CMakeLists.txt:33`

### Problem

`${CMAKE_CSS_STANDARD_LIBRARIES}` should be `${CMAKE_CXX_STANDARD_LIBRARIES}`. The variable expands to empty, silently losing the default standard libraries from the link line.

### Fix

Change `CSS` to `CXX`.

---

## Issue 46 -- `AudioSpace` ID Space: `uint16_t` Exhaustion -- OPEN

**Severity:** Medium
**Category:** Architecture
**Files:** `src/audio/source/lowl_audio_space.h`, `src/lowl_typedef.h`

### Problem

`AudioAssetId` and `AudioPlaybackId` are `uint16_t`, giving 65534 usable IDs (0 is invalid). Combined with Issue 20 (`advance_id` wraps to 0), the ID counter permanently stalls. Even with the free-list, if the free-list is empty when the counter wraps, the system is permanently broken. In a long-running game with frequent sound loading/unloading, 65534 total allocations can be reached.

### Fix

Widen to `uint32_t` for both types, and fix `advance_id` to skip 0 (Issue 20).

---

## Issue 47 -- `AudioSource::sample_rate` and `channel` Are Non-Const, Non-Atomic -- OPEN

**Severity:** Medium
**Category:** Thread Safety
**Files:** `src/audio/source/lowl_audio_source.h:40-41`

### Problem

These are set in the constructor and never modified, but they are not `const`. They are read from the audio thread without synchronization. The happens-before relationship depends on the caller's publication mechanism (e.g., the mixer's `ConcurrentQueue::enqueue`), which is implicit and undocumented.

### Fix

Declare as `const SampleRate sample_rate; const AudioChannel channel;` using the initializer list.

---

## Issue 48 -- `AudioStream` Ring Buffer Overflow on 32-bit -- OPEN

**Severity:** Medium
**Category:** Bug
**Files:** `src/audio/source/lowl_audio_stream.cpp`

### Problem

`read_position` and `write_position` are monotonically increasing `size_t` atomics. On a 32-bit system (`size_t` = 32 bits), at 48kHz, `size_t` wraps in ~24.8 hours. When `position % frame_capacity` is used, the modular arithmetic breaks if `frame_capacity` is not a power of 2.

### Fix

Add `static_assert(sizeof(size_t) >= 8, "...")`, or periodically re-base positions.

---

## Issue 49 -- WAV Reader: Unrecognized PCM Bit Depth Leaks `drwav` -- ALREADY FIXED

**Severity:** Medium
**Category:** Bug
**Files:** `src/audio/reader/lowl_audio_reader_wav.cpp:66-106`

### Problem

If `bytes_per_sample` does not match any case in the PCM switch, `sample_format` stays `Unknown`. This leads to a `return nullptr` in `create_audio_data` without ever calling `drwav_uninit(&wav)`.

### Fix

After the format switch, check if `sample_format == Unknown`, then `drwav_uninit(&wav)`, set error, and return.

---

## Issue 50 -- `create_audio_data`: `reinterpret_cast` Alignment Violation -- FIXED

**Severity:** Medium
**Category:** Bug (Undefined Behavior)
**Files:** `src/audio/reader/lowl_audio_reader.cpp:86, 94, 101, 108`

### Problem

The buffer is `unique_ptr<uint8_t[]>` (1-byte aligned). Code does `reinterpret_cast<const int32_t*>(p_buffer.get())` etc. Accessing through misaligned pointers is UB. Most allocators return 8+ byte aligned memory, but this is not guaranteed.

### Fix

Use `std::memcpy` into properly-typed locals in the conversion loop.

---

## Issue 51 -- Opus Reader: Does Not Set Error on Failure -- FIXED

**Severity:** Medium
**Category:** Bug
**Files:** `src/audio/reader/lowl_audio_reader_opus.cpp:14-19`

### Problem

`_error` from `op_open_memory` is captured but never inspected or propagated. The function returns `nullptr` with no error information.

### Fix

Map `_error` to an `ErrorCode` and call `error.set_error(...)`.

---

## Issue 52 -- MP3 / WAV: `DR_*_IMPLEMENTATION` Defines Risk ODR Violations -- OPEN

**Severity:** Medium
**Category:** Architecture
**Files:** `src/audio/reader/lowl_audio_reader_mp3.cpp:29`, `src/audio/reader/lowl_audio_reader_wav.cpp:5`

### Problem

`DR_MP3_IMPLEMENTATION` and `DR_WAV_IMPLEMENTATION` are defined before including the headers. If any other TU in the build (or a consumer) also defines them, all dr_libs functions get duplicate definitions. The MP3 reader's 23 `#define` symbol remappings are fragile -- new dr_mp3 symbols break them.

### Fix

Isolate each `_IMPLEMENTATION` define into a dedicated TU. Consider a namespace wrapper.

---

## Issue 53 -- ReSampler: No Null Check on `p_audio_data` -- FIXED

**Severity:** Medium
**Category:** Bug
**Files:** `src/audio/convert/lowl_audio_re_sampler_r8b.cpp:5`

### Problem

`p_audio_data` is a `shared_ptr` that could be null. Line 5 calls `p_audio_data->get_frame_count()` without checking.

### Fix

Add a null check and return `nullptr` early.

---

## Issue 54 -- Channel Converter: Potential Null Dereference -- ALREADY FIXED

**Severity:** Medium
**Category:** Bug
**Files:** `src/audio/convert/lowl_audio_channel_converter.cpp:42-49`

### Problem

If `frame_count == 0`, `storage` will be a `unique_ptr` wrapping a zero-size allocation (or null). `storage.get()` is called unconditionally. While the loop won't execute with `frame_count == 0`, the pointer arithmetic is still UB on a null pointer.

### Fix

Add an early return when `frame_count == 0`.

---

## Issue 55 -- CoreAudio: Audio Callback Reads Fields Without Memory Fence -- OPEN

**Severity:** Medium
**Category:** Thread Safety
**Files:** `src/audio/backend/coreaudio/lowl_audio_core_audio_device.cpp:74-93`

### Problem

The audio callback reads `audio_device_properties` and `audio_source` set by `start()` on the main thread. There is no explicit memory fence between the two threads. CoreAudio likely provides implicit ordering via `AudioOutputUnitStart`, but this is not guaranteed by the C++ memory model.

### Fix

Use `std::atomic` for `audio_source`, or document the ordering guarantee from CoreAudio's start function.

---

## Issue 56 -- CoreAudio: `property_callback` Only Processes First Address -- OPEN

**Severity:** Medium
**Category:** Bug
**Files:** `src/audio/backend/coreaudio/lowl_audio_core_audio_device.cpp:101-110`

### Problem

The callback receives `inNumberAddresses` but only inspects `inAddresses->mSelector` (index 0). Batched property change notifications are silently dropped.

### Fix

Iterate over all `inNumberAddresses` entries.

---

## Issue 57 -- CoreAudio: `create_description` Returns Zeroed Struct for Unsupported Formats -- OPEN

**Severity:** Medium
**Category:** Bug
**Files:** `src/audio/backend/coreaudio/lowl_audio_core_audio_device.cpp:529-538`

### Problem

For `U_INT_8` and `Unknown` formats, the function returns a default-initialized `AudioStreamBasicDescription` (all zeros) with no error signal. The caller passes this to `AudioUnitSetProperty`, which fails with an obscure error.

### Fix

Set an error code and return, or use `std::optional` to signal failure.

---

## Issue 58 -- WASAPI: STA Apartment vs. MTA Requirement -- OPEN

**Severity:** Medium
**Category:** Architecture
**Files:** `src/audio/backend/wasapi/lowl_audio_wasapi_com.cpp:14`

### Problem

COM is initialized with `COINIT_APARTMENTTHREADED` (STA). WASAPI event-driven callbacks typically require MTA. The code handles `RPC_E_CHANGED_MODE` but the fundamental STA choice is questionable.

### Fix

Use `COINIT_MULTITHREADED` or document the rationale.

---

## Issue 59 -- WASAPI: `start()` Leaks Resources on Error Paths -- OPEN

**Severity:** Medium
**Category:** Bug (Resource Leak)
**Files:** `src/audio/backend/wasapi/lowl_audio_wasapi_device.cpp:101-362`

### Problem

If `start()` fails after `Activate` but before completion, `audio_client`, handles, and other resources are leaked. No cleanup on early-return paths.

### Fix

Add SAFE_RELEASE/SAFE_CLOSE on each error path, or use a scope-guard pattern.

---

## Issue 60 -- `AudioDevice::render_to_device_buffer` Trusts Caller Buffer Size -- OPEN

**Severity:** Medium
**Category:** Bug
**Files:** `src/audio/backend/lowl_audio_device.cpp:81-111`

### Problem

The function takes `p_frames_per_buffer` and `p_bytes_per_frame` but trusts that the destination is large enough. If `write_sample` writes more bytes than expected (format mismatch), the function overwrites past the buffer. No bounds tracking on `write_ptr`.

### Fix

Track write position relative to buffer end and assert or clamp.

---

## Issue 61 -- `Buffer::get_available()` Can Underflow -- FIXED

**Severity:** Medium
**Category:** Bug
**Files:** `src/lowl_buffer.cpp:143`

### Problem

`virtual_length - position` where both are `size_t`. If `position > virtual_length` (due to a bug), this wraps to a huge number.

### Fix

`return position <= virtual_length ? virtual_length - position : 0;`

---

## Issue 62 -- `File::is_eof` Returns `false` When No File Is Open -- FIXED

**Severity:** Medium
**Category:** Semantic
**Files:** `src/lowl_file.cpp:93-98`

### Problem

If no file is open, `is_eof()` returns `false`. This is misleading -- a non-open file is not "not at EOF".

### Fix

Return `true` when no file is open, or add an `is_open()` method.

---

## Issue 63 -- `_INLINE_` Macro Uses Reserved Identifier Pattern -- OPEN

**Severity:** Medium
**Category:** Quality / UB
**Files:** `src/lowl_typedef.h:9-17`

### Problem

Names beginning with underscore followed by uppercase (`_INLINE_`) are reserved by the C++ standard for implementation use. Using them is technically UB and could conflict with compiler internals.

### Fix

Rename to `LOWL_INLINE` or `LOWL_ALWAYS_INLINE`.

---

## Issue 64 -- `#include <sal.h>` Placed Inside Logger Class Body -- OPEN

**Severity:** Medium
**Category:** Quality
**Files:** `src/lowl_logger.h:68, 71`

### Problem

`#include <sal.h>` directives are inside the class body in conditional compilation blocks. Headers included inside class scope could define macros that interfere with the class definition.

### Fix

Move these includes to the top of the file.

---

## Issue 65 -- `Buffer::write_data` Compares `size_t <= 0` -- FIXED

**Severity:** Medium
**Category:** Bug
**Files:** `src/lowl_buffer.cpp:23, 99`

### Problem

`p_length` is `size_t` (unsigned). The check `p_length <= 0` is equivalent to `p_length == 0` since unsigned values are never negative. This may trigger `-Wtautological-constant-in-range-compare` warnings.

### Fix

Change to `== 0`.

---

## Issue 66 -- CMake: `CMAKE_OSX_ARCHITECTURES` Set After `project()` -- OPEN

**Severity:** Medium
**Category:** Build
**Files:** `CMakeLists.txt:84-90`

### Problem

`CMAKE_OSX_ARCHITECTURES` must be set before `project()` to take effect. Setting it on line 86/89 after `project()` on line 2 may be too late. Also prevents building universal binaries.

### Fix

Move to a toolchain file or before `project()`.

---

## Issue 67 -- `LOWL_DEBUG` Defined as `PUBLIC` -- OPEN

**Severity:** Medium
**Category:** Build
**Files:** `CMakeLists.txt:125`

### Problem

`LOWL_DEBUG` is a `PUBLIC` compile definition, leaking debug-mode behavior into all consumers (tests, demo, and external projects linking this library).

### Fix

Change to `PRIVATE`.

---

## Issue 68 -- No Linux Audio Backend -- OPEN

**Severity:** Medium
**Category:** Architecture
**Files:** Project-wide

### Problem

The only real backends are CoreAudio (macOS) and WASAPI (Windows). There is no Linux backend. The `if (UNIX)` block in CMakeLists.txt only prints a message. PulseAudio, ALSA, or PipeWire support would be needed for Linux usability.

### Fix

Add at least one Linux backend (PipeWire is the modern choice).

---

## Issue 69 -- `Lib::terminate` Does Not Clear State or Allow Re-Init -- OPEN

**Severity:** Medium
**Category:** Architecture
**Files:** `src/lowl.cpp:43-47`

### Problem

`terminate()` tears down WASAPI COM but does not clear the `drivers` vector. After `terminate()`, `get_drivers()` returns stale pointers. Calling `initialize()` after `terminate()` is a no-op because `std::once_flag` has been consumed.

### Fix

Clear `drivers` in `terminate()`. Use a resettable init mechanism.

---

## Issue 70 -- Demo: `std::stoi` on User Input Without Exception Handling -- OPEN

**Severity:** Medium
**Category:** Bug
**Files:** `demo/main.cpp:156, 183, 235, 239`

### Problem

`std::stoi` throws `std::invalid_argument` or `std::out_of_range` on non-numeric input. No try/catch wraps these calls. The demo crashes on bad user input.

### Fix

Wrap in try/catch or use a non-throwing parser.

---

## Issue 71 -- `AudioSource` Redundant Atomic Initialization -- OPEN

**Severity:** Low
**Category:** Quality
**Files:** `src/audio/source/lowl_audio_source.h:34`, `src/audio/source/lowl_audio_source.cpp:9`

### Problem

`volume` is declared as `std::atomic<Volume> volume{}` (value-initialized to 0.0), then the constructor stores `DEFAULT_VOLUME` (1.0). The `{}` init is immediately overwritten.

### Fix

Use `std::atomic<Volume> volume{DEFAULT_VOLUME}` in the class definition.

---

## Issue 72 -- `AudioBlockView::channel()` Has No Bounds Check -- OPEN

**Severity:** Low
**Category:** Quality
**Files:** `src/audio/lowl_audio_buffer.h:17-23`

### Problem

`channel(p_channel)` indexes into `channels` array without checking `p_channel < channel_count`. Out-of-bounds access is UB.

### Fix

Add `assert(p_channel < channel_count)`.

---

## Issue 73 -- `AudioVoice` Defaults to `Playing`; `AudioSpace` Immediately Stops It -- OPEN

**Severity:** Low
**Category:** Quality
**Files:** `src/audio/source/lowl_audio_voice.cpp:12`, `src/audio/source/lowl_audio_space.cpp:119`

### Problem

The constructor sets `PlaybackState::Playing`, then `insert_playback_locked` immediately calls `stop_playback()`. This is wasted work. If `AudioVoice` is used directly, the default `Playing` state may cause unexpected immediate rendering.

### Fix

Default to `Stopped` state. Let `play()` transition to `Playing`.

---

## Issue 74 -- Inconsistent `get_frames_remaining` / `get_frame_count` Semantics -- OPEN

**Severity:** Low
**Category:** Semantic
**Files:** `src/audio/source/lowl_audio_mixer.cpp:396-406`, `src/audio/source/lowl_audio_space.cpp:488-498`

### Problem

Both `AudioMixer` and `AudioSpace` return `get_frames_remaining() == 1` and `get_frame_count() == 0`. This means `frames_remaining > frame_count`, which is nonsensical. The `1` is a sentinel to keep the device rendering, but it is undocumented.

### Fix

Document sentinel semantics for live/aggregate sources, or add an `is_live()` method.

---

## Issue 75 -- `detect_format` Is Extension-Only; No Magic-Byte Fallback -- OPEN

**Severity:** Low
**Category:** Quality
**Files:** `src/audio/reader/lowl_audio_reader.cpp:212-232`

### Problem

Format detection relies solely on file extension. A renamed file or a file with no extension will be misdetected.

### Fix

Fall back to magic-byte sniffing (RIFF, ID3/sync, fLaC, OggS).

---

## Issue 76 -- FLAC Reader: `DR_FLAC_NO_CRC` Disables Integrity Checks -- OPEN

**Severity:** Low
**Category:** Quality
**Files:** `src/audio/reader/lowl_audio_reader_flac.cpp:7`

### Problem

CRC validation is disabled. Corrupted FLAC data will produce garbled audio silently.

### Fix

Only define `DR_FLAC_NO_CRC` in performance-critical builds. Consider making it conditional.

---

## Issue 77 -- `AudioDeviceProperties::operator<` Inconsistent with Fuzzy `operator==` -- OPEN

**Severity:** Low
**Category:** Semantic
**Files:** `src/audio/backend/lowl_audio_device_properties.h:42-74`

### Problem

`operator==` uses `sample_rates_equal` (fuzzy comparison) but `operator<` uses plain `<`. Two rates that compare equal via `==` may not satisfy `!(a < b) && !(b < a)`, violating strict weak ordering for `std::sort`.

### Fix

Use exact comparison in both, or make `operator<` consistent with the fuzzy `==`.

---

## Issue 78 -- `AudioDriver::get_devices()` Returns Vector by Value -- OPEN

**Severity:** Low
**Category:** Performance
**Files:** `src/audio/backend/lowl_audio_driver.cpp:3`

### Problem

Returns `std::vector<std::shared_ptr<AudioDevice>>` by value, copying the vector on each call.

### Fix

Return `const std::vector<...>&`.

---

## Issue 79 -- Include Guards Missing `_H` Suffix -- OPEN

**Severity:** Low
**Category:** Quality
**Files:** `src/lowl_file_format.h`, `src/lowl_file.h`, `src/audio/lowl_audio_channel.h`, `src/audio/lowl_audio_sample_format.h`, `src/audio/lowl_audio_format.h`

### Problem

Include guards use names like `LOWL_FILE_FORMAT` instead of `LOWL_FILE_FORMAT_H`. These could collide with other macros.

### Fix

Add `_H` suffix.

---

## Issue 80 -- `File` Has Declared `test()` Method That Is Never Defined -- OPEN

**Severity:** Low
**Category:** Quality
**Files:** `src/lowl_file.h:16`

### Problem

`void test()` is declared as a private method but has no definition anywhere.

### Fix

Remove the declaration.

---

## Issue 81 -- No Compiler Warning Flags for GCC or MSVC -- OPEN

**Severity:** Low
**Category:** Build
**Files:** `CMakeLists.txt:130-184`

### Problem

Compiler flags are only set for Clang. GCC builds have no warnings beyond defaults. MSVC has no `/W4` or `/WX`.

### Fix

Add `-Wall -Wextra -Werror` for GCC and `/W4 /WX` for MSVC.

---

## Issue 82 -- Demo: Global Variables, Inconsistent Arg Parsing, No `--help` -- OPEN

**Severity:** Low
**Category:** Quality
**Files:** `demo/main.cpp`

### Problem

`music_paths`, `device_index`, etc. are global variables. Argument parsing uses inconsistent prefix checks (`find` vs `rfind`). No `--help` flag or usage message. Range-for loops copy values by value instead of const reference.

### Fix

Wrap state in a config struct, add `--help`, use `const auto &` in range-for.

---

## Issue 83 -- Test Coverage Gaps -- OPEN

**Severity:** Low
**Category:** Testing

### Missing Coverage

1. **`File` class** -- no tests at all (open, read, seek, EOF, error paths)
2. **`Timer` class** -- no tests (interval, stop, stop-from-callback, destructor)
3. **`ReleasePool` class** -- no tests (add/release cycle, timer cleanup, thread safety)
4. **`Buffer` error paths** -- no tests for read past end, underflow, growth behavior
5. **`Error` class** -- no tests for vendor errors, clear, text formatting
6. **`AudioData::create_slice` edge cases** -- negative seconds, zero length, single channel
7. **`AudioStream` capacity exhaustion** -- no test for writing to a full ring buffer
8. **`AudioStream` concurrent read/write** -- the SPSC guarantee is untested
9. **`AudioMixer` limits** -- no test for >1024 sources, ack owner lifecycle, deduplication
10. **`Lib` static API** -- `create_data`, `detect_format` untested through the facade
11. **`AudioSpace` thread safety** -- concurrent `render()` + `play()`/`stop()` untested
12. **Volume/panning** -- `process_volume` with non-default values untested; multichannel panning untested
13. **Test utilities** -- `StereoSample` / `make_stereo_audio_data` duplicated across 5 test files; should be a shared header

---

## Suggested Execution Sequence

| Phase | Issues | Rationale |
|-------|--------|-----------|
| **1 -- Resource leaks & UB** | #1, #2, #3, #4, #17 | Silent data corruption and memory leaks in readers/converters. Every file load is affected. |
| **2 -- Platform backend bugs** | #9, #10, #11, #12, #13, #14, #32-38 | WASAPI and CoreAudio have critical leaks, crashes, and incorrect API usage. |
| **3 -- Core infrastructure** | #5, #6, #7, #8 | Timer, ReleasePool, and Lib initialization have use-after-free and error-swallowing bugs. |
| **4 -- Source layer semantics** | #16, #20, #22, #23, #24, #25, #46 | Mixer lifetime safety, ID exhaustion, and state transition correctness. |
| **5 -- Remaining high issues** | #18, #19, #21, #26-31, #39-42 | Negative time UB, NaN propagation, reader overruns, logger filtering. |
| **6 -- Build & API** | #15, #43-45, #44, #66, #67 | Dead C API, CMake issues, demo bug. |
| **7 -- Medium issues** | #47-70 | Thread safety documentation, architecture gaps, error handling. |
| **8 -- Quality & testing** | #71-83 | Code quality, test coverage, documentation. |

---

## Success Criteria

- No resource leaks in any reader (WAV, FLAC, MP3, OGG, Opus).
- No undefined behavior in sample conversion for any input value.
- `Timer` and `ReleasePool` do not use-after-free on destruction.
- `Lib::initialize` errors are visible to callers.
- WASAPI devices have correct properties, no heap overflow, no COM leaks.
- CoreAudio `start()`/`stop()` are fully symmetric with complete cleanup.
- ID space does not permanently exhaust after 65534 allocations.
- `AudioMixer` public API documents lifetime requirements.
- All critical and high issues resolved; tests pass.
