# lowl_audio v2 -- Current Open Issues

Generated from an audit of `issuev2.md` against the current tree on 2026-03-30.

Notes:
- Issue 57 is no longer included here because `CoreAudioDevice::create_description()` now reports unsupported formats explicitly instead of returning a zeroed `AudioStreamBasicDescription`.
- Issue 44 is no longer included here because the repo policy explicitly accepts `cmake_minimum_required(VERSION 3.31)`.
- Issue 58 is no longer included here because the library-wide WASAPI COM bootstrap now prefers `COINIT_MULTITHREADED`, matching the render callback thread's COM model.
- Issue 59 is no longer included here because `WasapiDevice::start()` now routes partial-start failures through centralized cleanup.
- `issuev2.md` still has stale section headings for Issues 46, 58, 59, and 60. Their current status in that file no longer matches the current tree.

## Current Unresolved Set

| #  | Status | Severity | Category | Subsystem | Title |
|----|--------|----------|----------|-----------|-------|
| 24 | OPEN | High | Thread | Source | `AudioVoice` compound state transitions observable in intermediate states |
| 46 | DEFERRED | Medium | Architecture | Source | `AudioSpace` ID space: `uint16_t` exhaustion after 65534 allocations |
| 60 | DEFERRED | Medium | Bug | Backend | `AudioDevice::render_to_device_buffer` trusts caller buffer size |
| 68 | OPEN | Medium | Architecture | Architecture | No Linux audio backend (PulseAudio / ALSA / PipeWire) |
| 73 | DEFERRED | Low | Quality | Source | `AudioVoice` defaults to `Playing`; `AudioSpace` immediately stops it |
| 74 | DEFERRED | Low | Semantic | Source | `get_frames_remaining()` returns 1 while `get_frame_count()` returns 0 |

## Issue 24 -- `AudioVoice` Compound State Transitions Observable in Intermediate States

**Status:** OPEN  
**Severity:** High  
**Category:** Thread Safety  
**Files:** `src/audio/source/lowl_audio_voice.h`, `src/audio/source/lowl_audio_voice.cpp`

### Validation

`AudioVoice` still publishes queryable state through separate atomics: `reported_position`, `pending_seek_position`, `detached`, and `playback_state`. `restart_playback()`, `stop_playback()`, `seek_frame()`, and the render path update these independently with no coherent published snapshot. That means a reader can still observe a mixed state across those fields.

### Problem

`restart_playback()` and `stop_playback()` are multi-step state transitions. Querying code can still observe intermediate states like "playing with stale position" or "stopped with old position" because the public state is not published atomically as one unit.

### Proposed Fix

Keep the internal render cursor as render-thread state, but publish one coherent snapshot for externally visible state such as `position`, `playback_state`, and `detached`.

---

## Issue 46 -- `AudioSpace` ID Space: `uint16_t` Exhaustion

**Status:** DEFERRED  
**Severity:** Medium  
**Category:** Architecture  
**Files:** `src/lowl_typedef.h`, `src/audio/source/lowl_audio_space.cpp`, `src/audio/source/lowl_audio_asset_handle.h`, `src/audio/source/lowl_audio_playback_handle.h`

### Validation

`AudioAssetId` and `AudioPlaybackId` are still typedefed to `uint16_l`, and `AudioSpace` still advances those IDs through `advance_id()` while storing them in bounded handle slots. Issue 20 fixed the zero-wrap bug, but the overall 16-bit allocation space is unchanged.

### Problem

Long-running applications can still exhaust the available asset or playback IDs after enough total allocations, even if slots are reused most of the time.

### Proposed Fix

Widen `AudioAssetId` and `AudioPlaybackId` to `uint32_t` and carry that change through the public handle types and lookup code.

---

## Issue 60 -- `AudioDevice::render_to_device_buffer` Trusts Caller Buffer Size

**Status:** DEFERRED  
**Severity:** Medium  
**Category:** Bug  
**Files:** `src/audio/backend/lowl_audio_device.h`, `src/audio/backend/lowl_audio_device.cpp`

### Validation

`render_to_device_buffer()` still takes only `p_dst`, `p_frames_per_buffer`, and `p_bytes_per_frame`. It computes `total_bytes` from those values and writes through `write_ptr`, but the API still has no explicit destination-buffer byte length to validate against. CoreAudio now adds a caller-side size guard, but the shared helper itself still relies on the caller contract.

### Problem

The helper cannot enforce bounds because the API does not carry the destination span size. That leaves the shared render path dependent on every caller getting the size math right.

### Proposed Fix

Redesign the helper to accept an explicit output span or byte count and enforce writes against that bound.

---

## Issue 68 -- No Linux Audio Backend

**Status:** OPEN  
**Severity:** Medium  
**Category:** Architecture  
**Files:** `CMakeLists.txt`, `src/lowl.cpp`, `src/audio/backend/*`

### Validation

The project still only contains dummy, CoreAudio, and WASAPI backends. The UNIX path in `CMakeLists.txt` still just logs `"[LOWL] UNIX"`, and `Lib::initialize()` still has no Linux backend to register.

### Problem

The library is still not a real Linux audio solution. On Linux it has no platform backend for actual audio output.

### Proposed Fix

Add a Linux backend such as PipeWire, ALSA, or PulseAudio.

---

## Issue 73 -- `AudioVoice` Defaults to `Playing`; `AudioSpace` Immediately Stops It

**Status:** DEFERRED  
**Severity:** Low  
**Category:** Quality  
**Files:** `src/audio/source/lowl_audio_voice.cpp`, `src/audio/source/lowl_audio_space.cpp`

### Validation

`AudioVoice` still sets `playback_state` to `Playing` in its constructor, and `AudioSpace::insert_playback_locked()` still immediately calls `slot.voice->stop_playback()` when a playback slot is created.

### Problem

That remains wasted work inside `AudioSpace`, and direct `AudioVoice` users still inherit an implicit "starts playing immediately" default.

### Proposed Fix

Start `AudioVoice` in a stopped state and require an explicit playback start, but only as part of a deliberate public-behavior review.

---

## Issue 74 -- Inconsistent `get_frames_remaining` / `get_frame_count` Semantics

**Status:** DEFERRED  
**Severity:** Low  
**Category:** Semantic  
**Files:** `src/audio/source/lowl_audio_mixer.cpp`, `src/audio/source/lowl_audio_space.cpp`

### Validation

`AudioMixer::get_frames_remaining()` and `AudioSpace::get_frames_remaining()` still return `1`, while their `get_frame_count()` implementations still return `0`.

### Problem

The sentinel keeps aggregate/live sources rendering, but it preserves an API contract where `frames_remaining > frame_count`, which is semantically inconsistent and still undocumented.

### Proposed Fix

Either document the sentinel semantics explicitly or add a dedicated live/streaming query instead of encoding that state through frame-count methods.
