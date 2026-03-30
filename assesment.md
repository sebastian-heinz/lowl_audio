# Audio Library Assessment

## Scope

Reviewed the core render path and lifetime/control path in:

- `src/lowl.cpp`
- `src/audio/backend/lowl_audio_device.*`
- `src/audio/backend/coreaudio/lowl_audio_core_audio_device.*`
- `src/audio/backend/wasapi/lowl_audio_wasapi_device.*`
- `src/audio/source/lowl_audio_source.*`
- `src/audio/source/lowl_audio_mixer.*`
- `src/audio/source/lowl_audio_space.*`
- `src/audio/source/lowl_audio_voice.*`
- `src/audio/source/lowl_audio_stream.*`
- `src/audio/source/lowl_audio_data.*`
- `src/audio/convert/lowl_audio_sample_converter.h`
- `src/lowl_logger.*`

## 1. Architecture

### Main object graph

```text
Lowl::Lib
  -> AudioDriver
       -> CoreAudioDriver / WasapiDriver / DummyDriver
            -> AudioDevice
                 -> CoreAudioDevice / WasapiDevice
                      -> RenderState
                           - AudioDeviceProperties
                           - shared_ptr<AudioSource>
                           - reusable AudioBuffer
```

```text
AudioSource
  -> AudioMixer
  -> AudioSpace
  -> AudioVoice
  -> AudioStream
```

### Relationships

- `Lowl::Lib` is the static entry point. It initializes drivers, creates readers, creates `AudioData`, and returns the default device.
- `AudioDriver` owns enumerated `AudioDevice` instances for a backend.
- `AudioDevice` is the backend-neutral output abstraction. Its backend callback calls `render_to_device_buffer()`, which renders from an `AudioSource` into a reusable planar float buffer and then converts/interleaves into the device buffer.
- `AudioSource` is the common renderable interface.
- `AudioMixer` is the real aggregator. It mixes multiple active `AudioSource*` objects into one output stream. Control-thread add/remove work is sent through a bounded MPSC `AudioMixerEvent` queue, and terminal acknowledgements are returned through a per-owner bounded MPSC `AudioMixerAck` queue shared by render-thread completions and control-thread immediate rejections.
- `AudioSpace` is a higher-level asset/playback manager wrapped around an internal `AudioMixer`. It owns:
  - decoded assets in `AudioData`
  - live playback slots containing `AudioVoice`
  - handle tables for asset, playback, and mixer ownership
- `AudioVoice` plays immutable decoded clip data (`shared_ptr<const AudioData>`) with per-voice transport, gain, and panning.
- `AudioStream` is a lock-free ring-buffer source for pushed sample data.
- `AudioData` is immutable planar PCM storage created by readers/converters/resamplers off the audio thread.

### Effective render chain

Typical playback path:

```text
CoreAudio/WASAPI callback
  -> AudioDevice::render_to_device_buffer()
       -> AudioSpace::render()
            -> AudioMixer::render()
                 -> AudioVoice::render() / AudioStream::render()
```

## 2. Real-time Safety Assessment

### What is good

- No file I/O or decoder work happens in the audio callback. File loading, decode, resample, and channel conversion happen on the control thread in `AudioSpace::add_audio()` and the reader/converter layer.
- The steady-state source render path avoids the big control mutexes:
  - `AudioSpace::render()` does not take `state_mutex` (`src/audio/source/lowl_audio_space.cpp:572-578`).
  - `AudioVoice::render()` does not take `control_state_mutex` (`src/audio/source/lowl_audio_voice.cpp:23-105`).
  - `AudioStream::render()` is atomic/ring-buffer based.
- The mixer callback/control handoff is now bounded and non-allocating on the render thread:
  - control-thread submissions use `events.try_enqueue()` into a bounded MPSC queue
  - terminal acknowledgements use a per-owner bounded MPSC queue
  - submission failures are checked and surfaced explicitly instead of relying on a potentially allocating queue path
- Backend callbacks do not do file I/O.

### What is not real-time safe enough

- WASAPI debug logging is on the audio thread (`src/audio/backend/wasapi/lowl_audio_wasapi_device.cpp:426-547`). In debug builds the logger takes a recursive mutex and allocates/format strings (`src/lowl_logger.cpp:44-61`, `src/lowl_logger.cpp:86-100`, `src/lowl_logger.h:81-101`).

### Locking/synchronization summary

- `state_mutex` in `AudioSpace` and `control_state_mutex` in `AudioVoice` are kept off the render path. That part is good.
- The callback path is now atomic/queue based without callback-thread allocation.
- The mixer still uses raw `AudioSource*` pointers in the render thread. That lifetime contract is now simpler than before because acknowledgements flow through one queue topology instead of mixed queue/side-channel paths.

## 3. Issues And Bugs

### Medium: zero-frame render removes a voice

Files:

- `src/audio/source/lowl_audio_voice.cpp:57-66`

Problem:

- `AudioVoice::render()` treats `p_block.frame_count == 0` the same as end-of-stream and returns `RenderState::Remove`.

Impact:

- A zero-frame callback should be a no-op. As written, a backend that ever invokes a 0-frame render can accidentally retire a playing voice.

Recommendation:

- Handle zero-frame blocks as `{0, RenderState::Ok}` or `{0, RenderState::Starved}` and leave transport state unchanged.

### Medium: published transport position is truncated to 32 bits

Files:

- `src/audio/source/lowl_audio_voice.h:27-31`
- `src/audio/source/lowl_audio_voice.h:45-60`
- `src/audio/source/lowl_audio_voice.cpp:100-102`
- `src/audio/source/lowl_audio_voice.cpp:160-162`

Problem:

- `render_position` is `size_t`, but the published snapshot stores `position` as `uint32_t`.

Impact:

- Long clips wrap reported position after `2^32 - 1` frames.
- At 48 kHz that is about 24.8 hours; at higher rates it wraps sooner.
- `get_frame_position()` / `get_frames_remaining()` can report incorrect values for long assets.

Recommendation:

- Store the published position in 64 bits as well, even if the packed state becomes larger.

### Low: CoreAudio callback does not zero the output buffer when render state is missing

Files:

- `src/audio/backend/coreaudio/lowl_audio_core_audio_device.cpp:97-100`

Problem:

- If `published_state == nullptr`, the callback returns `noErr` without explicitly silencing `ioData`.

Impact:

- During teardown/restart windows this can leave stale buffer contents visible to the backend instead of deterministic silence.

Recommendation:

- If `ioData` is valid, zero the provided buffers before returning.

### Low: debug logging on the WASAPI audio thread is not RT-safe

Files:

- `src/audio/backend/wasapi/lowl_audio_wasapi_device.cpp:426-547`
- `src/lowl_logger.cpp:44-61`
- `src/lowl_logger.cpp:86-100`

Problem:

- The WASAPI thread logs from inside the callback loop and error paths.
- In `LOWL_DEBUG`, logging takes locks and allocates.

Impact:

- Mostly a debug-build problem, but it is still enough to perturb timing and mask real XRUN behavior.

Recommendation:

- Replace callback-thread logging with lock-free counters, deferred diagnostics, or one-shot state flags drained by a non-RT thread.

## 4. Hot Path And Performance Assessment

### Hottest path

The main CPU path is:

1. `AudioDevice::render_to_device_buffer()` (`src/audio/backend/lowl_audio_device.cpp:91-156`)
2. `AudioMixer::render()` / `render_mixed_block()` (`src/audio/source/lowl_audio_mixer.cpp:202-320`)
3. Per-source gain/pan in `AudioSource::process_volume()` and `process_panning()` (`src/audio/source/lowl_audio_source.cpp:60-95`)
4. Per-sample format conversion in `SampleConverter::write_sample()` (`src/audio/convert/lowl_audio_sample_converter.h:86-146`)

### Main costs

- Redundant clearing:
  - device scratch buffer is cleared every callback (`src/audio/backend/lowl_audio_device.cpp:131-133`)
  - mixer output block is cleared again (`src/audio/source/lowl_audio_mixer.cpp:317-320`)
  - mixer scratch buffer is cleared once per active source (`src/audio/source/lowl_audio_mixer.cpp:217`)
- Multiple full-buffer passes:
  - voice/stream render
  - per-source gain
  - per-source pan
  - mixer accumulation
  - device interleave/convert
- Expensive scalar conversion:
  - `render_to_device_buffer()` clamps every sample (`src/audio/backend/lowl_audio_device.cpp:144-145`)
  - `write_sample()` then switches on format for every sample (`src/audio/convert/lowl_audio_sample_converter.h:86-146`)
  - integer formats clamp/round again internally (`src/audio/convert/lowl_audio_sample_converter.h:51-64`)
- `AudioSpace::render()` pushes master gain/pan into the mixer every callback (`src/audio/source/lowl_audio_space.cpp:576-577`), which is small but unnecessary steady-state traffic.

### Likely bottleneck profile

This code is more memory-bandwidth and buffer-pass limited than lock limited:

- many clears
- many read/modify/write passes
- scalar sample conversion in the backend handoff

Synchronization is not the main bottleneck here; the hot path is dominated by buffer clearing, repeated passes, and scalar conversion.

### Performance recommendations

1. Add format-specialized backend write paths:
   - direct memcpy/interleave fast path for `FLOAT_32`
   - specialized loops for `INT_16`
   - avoid per-sample switch dispatch
2. Tighten the source render contract so produced frames are fully written, then remove redundant pre-clears where possible.
3. Cache panning gains when panning changes instead of recomputing `sqrt()` every render (`src/audio/source/lowl_audio_source.cpp:80-93`).
4. Consider fusing gain/pan into mix accumulation for voices/streams so each sample is touched fewer times.
5. If `AudioSpace` master gain/pan must remain, apply them as a final post-mix stage instead of store-forwarding into `AudioMixer` every callback.

## 5. Overall Assessment

The overall structure is sensible: decode/convert work is off the callback thread, sources are composable, and the render path mostly avoids coarse locks. The mixer handoff is now materially stronger and simpler because both events and acknowledgements use bounded non-allocating queue paths with matching producer/consumer topology. The remaining weaknesses are hot-path efficiency and a few correctness bugs around voice/device edge cases.
