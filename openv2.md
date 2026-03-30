# lowl_audio v2 -- Current Open Issues

Generated from an audit of `issuev2.md` against the current tree on 2026-03-30.

Notes:
- Issue 24 is no longer included here because `AudioVoice` now publishes externally visible state through one coherent snapshot, and the threaded regression test covers the stale `Stopped` + nonzero-position case.
- Issue 57 is no longer included here because `CoreAudioDevice::create_description()` now reports unsupported formats explicitly instead of returning a zeroed `AudioStreamBasicDescription`.
- Issue 44 is no longer included here because the repo policy explicitly accepts `cmake_minimum_required(VERSION 3.31)`.
- Issue 58 is no longer included here because the library-wide WASAPI COM bootstrap now prefers `COINIT_MULTITHREADED`, matching the render callback thread's COM model.
- Issue 59 is no longer included here because `WasapiDevice::start()` now routes partial-start failures through centralized cleanup.
- Issue 60 is no longer included here because `AudioDevice::render_to_device_buffer()` now accepts an explicit destination byte size, rejects undersized output buffers in the shared helper, and the regression test covers the short-buffer path.
- Issue 73 is no longer included here because `AudioVoice` now starts stopped and `AudioSpace` no longer immediately stops a newly created playback slot.
- Issue 74 is no longer included here because `AudioMixer` and `AudioSpace` now use a consistent one-frame live-source sentinel for both `get_frames_remaining()` and `get_frame_count()`, and that behavior is documented.
- `issuev2.md` still has stale section headings for Issues 46, 58, and 59. Their current status in that file no longer matches the current tree.

## Current Unresolved Set

| #  | Status | Severity | Category | Subsystem | Title |
|----|--------|----------|----------|-----------|-------|
| 46 | DEFERRED | Medium | Architecture | Source | `AudioSpace` ID space: `uint16_t` exhaustion after 65534 allocations |
| 68 | OPEN | Medium | Architecture | Architecture | No Linux audio backend (PulseAudio / ALSA / PipeWire) |

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
