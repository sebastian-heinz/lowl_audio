# LowL Audio - Library Review & Analysis

## Project Overview

**LowL Audio** is a C++17 low-latency audio library designed for real-time applications (games, DAWs, interactive media). It provides a platform-agnostic abstraction over native audio APIs (CoreAudio on macOS, WASAPI on Windows) with a composable source pipeline, lock-free threading, and multi-format codec support.

**Core stats:** ~75 source files, 9 third-party dependencies (all permissive licenses), CMake 3.31+, supports WAV/MP3/FLAC/OGG/Opus.

### Architecture (bottom-up)

```
Platform APIs (CoreAudio, WASAPI)
    -> AudioDevice (hardware abstraction)
        -> AudioSource pipeline (composable chain)
            -> AudioData (pre-loaded buffers)
            -> AudioStream (lock-free queue streaming)
            -> AudioMixer (blend N sources, lock-free events)
            -> AudioSpace (game-oriented ID-based sound manager)
            -> ReSamplerSource (on-the-fly rate conversion wrapper)
        -> Readers (WAV, MP3, FLAC, OGG, Opus -> AudioData)
        -> Converters (sample format, channel layout, sample rate)
```

---

## What Is Good

### 1. Lock-Free Real-Time Audio Design
The library correctly avoids locks on the audio callback thread. `AudioMixer` uses `moodycamel::ConcurrentQueue` for event-driven source management, and `AudioStream` uses `moodycamel::ReaderWriterQueue` for SPSC streaming. Atomic operations for volume/panning/play-state are validated at compile time via `static_assert(std::atomic<Sample>::is_always_lock_free)`. This is the single most important design decision for a real-time audio library and it's done right.

### 2. Composable AudioSource Pipeline
The decorator/chain pattern (`AudioSource -> ReSamplerSource -> AudioMixer -> AudioDevice`) is elegant. Each layer adds functionality without modifying lower layers. This makes the library highly extensible - adding effects, filters, or spatial audio is just adding another `AudioSource` wrapper.

### 3. Platform Abstraction Quality
The CoreAudio and WASAPI backends are well-structured with proper factory construction (`_constructor_tag` pattern), thorough error propagation with vendor error codes, and correct API usage (AudioUnit callbacks, WASAPI event-driven rendering). The compile-time driver selection via `#ifdef` keeps binary size lean.

### 4. Codec Coverage
Supporting WAV, MP3, FLAC, OGG Vorbis, and Opus with header-only/lightweight libraries (dr_libs) is practical. The unified `AudioReader` factory pattern makes adding new formats trivial.

### 5. Game-Friendly AudioSpace
`AudioSpace` with `SpaceId`-based management, automatic resampling/channel conversion on load, and fire-and-forget `play(id)` / `stop(id)` is a well-thought-out game audio API. Pre-processing on load means zero conversion overhead during playback.

### 6. Type Safety
Strong typedefs (`Sample`, `Volume`, `Panning`, `SampleRate`, `SpaceId`), scoped enums throughout, protected constructor tag pattern, and the configurable `LOWL_TYPE_SAMPLE_64` for double-precision pipelines show attention to correctness.

---

## What Is Bad

### 1. AudioFrame Is Hard-Coded to Stereo
`AudioFrame` has only `left` and `right` fields with `MAX_CHANNEL = 2`. The comments describe Quad and 5.1 layouts but there's no actual support. The `operator[]` only returns `left` or `right` regardless of index, meaning any channel > 1 silently returns `right`. This is a fundamental limitation that will require a breaking redesign.

### 2. Incomplete / Stub Implementations
- `get_closest_properties()` has an empty `for` loop and always returns `properties_list[0]` (the TODO has been there presumably since the beginning)
- `uint8_to_float()` returns `0` unconditionally
- `write_sample()` for `FLOAT_64` is a no-op (silent data loss)
- `process_panning()` has identical code for Mono/Stereo/Quadraphonic with a TODO
- `start_stop_callback()` and `property_callback()` in CoreAudio are empty
- Exclusive mode (device hogging) is fully commented out
- `ReSamplerSource::max_frames` is set to 100 but never used

### 3. Error Object Passed by Value in ChannelConverter
`ChannelConverter::convert()` takes `Error error` (by value, not reference). Errors set inside the function are silently lost - the caller never sees them. This is a critical bug that makes channel conversion silently fail:
```cpp
// BUG: Error passed by value - caller never sees the error!
std::unique_ptr<AudioData> convert(AudioChannel p_to,
    std::shared_ptr<AudioData> p_audio_data,
    Error error) const;  // should be Error &error
```

### 4. No Clipping/Saturation in Mixer
`AudioMixer::read()` sums frames with `audio_frame += read_frame` without any clipping or normalization. Playing 10 sounds simultaneously will produce values well above 1.0, causing distortion or undefined behavior when converted to integer formats at the device boundary. The device does clamp individual samples, but the summed frame can still wrap in integer conversion.

### 5. Thread Safety Gaps
- `AudioSource::name` (std::string) is read/written without synchronization
- `AudioSource::sample_rate` and `channel` are non-atomic protected members read from the audio thread
- `AudioData::size` is non-atomic but `position` is atomic - there's an implicit assumption size never changes
- `Lib::drivers` (static vector) has no synchronization between `initialize()` and `get_drivers()`/`get_default_device()`

### 6. Memory Allocation in Audio Callbacks
`AudioMixer::read()` can trigger `std::vector` reallocation when processing `Remove` events (via `sources.erase` and the erase-remove idiom). Vector reallocation is not real-time safe. The mixer also does `sources.push_back()` when processing `Mix` events.

---

## Novel Ideas

### 1. Protected Constructor Tag Pattern
Using an empty struct `_constructor_tag` with explicit default constructor to prevent public construction while allowing factory methods is a clean C++ pattern that avoids the usual `friend` mess:
```cpp
struct _constructor_tag { explicit _constructor_tag() = default; };
AudioDevice(_constructor_tag);  // only accessible to subclasses/factories
```

### 2. Atomic Flag for Seek/Reset Synchronization
`AudioData` uses `std::atomic_flag` (`is_not_reset`) as a lightweight signal between the control thread (calling `reset()`/`seek()`) and the audio thread (calling `read()`). The flag acts as a "dirty bit" that's checked-and-set atomically, avoiding the need for a mutex on the hot path.

### 3. ReleasePool with Timer-Based Cleanup
`ReleasePool` holds `shared_ptr<void>` references and periodically (every 10s) removes objects whose use count has dropped to 1. This defers destruction off the real-time thread - a common pattern in audio but nicely implemented here with a generic template.

### 4. AudioSpace as Pre-Processed Sound Bank
The concept of a "Space" that eagerly resamples and converts all audio to a uniform format at load time, then provides zero-overhead playback via ID, is a game-audio-specific optimization that eliminates runtime conversion overhead entirely.

---

## Features That Would Make Good Additions

### 1. Effects/DSP Pipeline
An `AudioEffect` base class (gain, EQ, reverb, compressor, delay) that slots into the `AudioSource` chain would be the most impactful addition. The composable architecture already supports this naturally.

### 2. 3D Spatial Audio
Distance attenuation, HRTF, and doppler effect support. The panning system already exists; extending it to 3D coordinates would make this viable for games.

### 3. Audio Bus/Group System
Named buses (SFX, Music, Voice, Master) with per-bus volume/effects, similar to Unity/FMOD. `AudioSpace` is close to this but lacks hierarchical routing.

### 4. Streaming Decoder
Currently all audio is fully decoded into memory (`AudioData`). A streaming decoder that reads from disk in chunks (especially for music/long audio) would reduce memory usage dramatically.

### 5. ALSA/PulseAudio/PipeWire Linux Backend
Linux is the only major desktop platform without a backend.

### 6. Fade In/Out and Crossfade
Programmatic envelope control (linear/exponential fades) for smooth transitions. Currently volume changes are instantaneous, causing clicks.

### 7. Looping with Loop Points
`AudioData` currently resets to 0 on end and returns `Remove`. Configurable loop behavior (loop forever, loop N times, loop between points A and B) is essential for game audio.

### 8. Event/Callback System
Notifications for: playback finished, loop point reached, buffer underrun, device hot-plug. The `start_stop_callback` and `property_callback` stubs show this was planned.

---

## Worth Pointing Out

- **ErrorCode inconsistency**: `ReaderNotFound = 205` and `ReaderNoAudioData = 206` are positive while all others are negative. This looks like a typo that could break `has_error()` if it checks sign.
- **`_INLINE_` macro**: Uses `__attribute__((always_inline))` / `__forceinline` which are compiler hints that can hurt performance on cold paths by increasing code size. Regular `inline` would suffice for most methods.
- **CMake 3.31 requirement** is extremely high (released late 2024). Most projects target 3.16-3.22. This unnecessarily limits adoption.
- **`std::clamp<size_t>(p_frame, 0, size - 1)`** in `AudioData::seek_frame()`: if `size` is 0, `size - 1` wraps to `SIZE_MAX` due to unsigned underflow.
- **`sample_to_int8` variable named `int16`**: Copy-paste naming error that hurts readability.
- **Silence filling bug in `write_frames()`**: The inner loop iterates `p_bytes_per_frame` times per *sample*, but `p_bytes_per_frame` represents bytes per *frame* (which is samples * bytes_per_sample). The zeroing logic writes too many or too few zero bytes.
- **`AudioMixerEvent::type` uses raw uint8_t** instead of an enum class, inconsistent with the rest of the codebase's type-safe style.
- **`read_frames()` copies `std::vector<float> samples` by value** in the function signature. For large audio files this copies the entire decoded sample buffer unnecessarily.

---

## Critical Code Analysis & Improvement Suggestions

### 1. ChannelConverter Error Parameter Bug (Critical)

**Problem:** `Error` passed by value means errors are silently lost.

**Current:**
```cpp
// lowl_audio_channel_converter.h:24,27
std::vector<AudioFrame> convert(AudioChannel p_from, AudioChannel p_to,
    std::vector<AudioFrame> audio_data, Error error) const;  // BUG

std::unique_ptr<AudioData> convert(AudioChannel p_to,
    std::shared_ptr<AudioData> p_audio_data, Error error) const;  // BUG
```

**Fix:**
```cpp
std::vector<AudioFrame> convert(AudioChannel p_from, AudioChannel p_to,
    std::vector<AudioFrame> audio_data, Error &error) const;  // pass by reference

std::unique_ptr<AudioData> convert(AudioChannel p_to,
    std::shared_ptr<AudioData> p_audio_data, Error &error) const;  // pass by reference
```

**Why:** Without this fix, any channel conversion error (e.g., unsupported Quadraphonic conversion) silently succeeds, returning unconverted data. The caller in `AudioSpace::add_audio()` checks `error.has_error()` but will never see the error.

---

### 2. Mixer Clipping / Normalization (High)

**Problem:** Summing N sources without attenuation produces values >> 1.0, causing hard clipping artifacts.

**Current (lowl_audio_mixer.cpp:44):**
```cpp
audio_frame += read_frame;  // unbounded summation
```

**Suggested fix - simple peak limiter:**
```cpp
// After summing all sources:
Sample peak = std::max(std::abs(audio_frame.left), std::abs(audio_frame.right));
if (peak > AudioFrame::MAX_SAMPLE_VALUE) {
    Sample attenuation = AudioFrame::MAX_SAMPLE_VALUE / peak;
    audio_frame *= attenuation;
}
```

**Why:** Without limiting, playing 4 sounds at full volume produces samples at 4.0. The device's `std::clamp` in `write_frames` produces hard clipping. A peak limiter preserves relative levels. A more sophisticated approach would use a compressor with attack/release, but a simple limiter is a pragmatic first step.

---

### 3. ReSamplerSource Drains Entire Source Per Read (High)

**Problem:** `ReSamplerSource::read()` drains *all* frames from the source in a single call, then tries to read one output frame. For large `AudioData` (millions of frames), this blocks the audio callback for the entire duration of resampling.

**Current (lowl_audio_re_sampler_source.cpp:6-16):**
```cpp
ReadResult ReSamplerSource::read(AudioFrame &audio_frame) {
    AudioFrame frame{};
    while (audio_source->read(frame) == ReadResult::Read) {  // reads ALL frames
        re_sampler->write(frame);
    }
    if (re_sampler->read(audio_frame)) {
        return ReadResult::Read;
    }
    return ReadResult::End;
}
```

**Fix - feed frames incrementally:**
```cpp
ReadResult ReSamplerSource::read(AudioFrame &audio_frame) {
    // First, try to read from resampler output buffer
    if (re_sampler->read(audio_frame)) {
        return ReadResult::Read;
    }

    // Feed source frames until resampler produces output
    AudioFrame frame{};
    while (true) {
        ReadResult result = audio_source->read(frame);
        if (result != ReadResult::Read) {
            // Source exhausted, flush resampler
            re_sampler->flush();
            if (re_sampler->read(audio_frame)) {
                return ReadResult::Read;
            }
            return result;
        }
        re_sampler->write(frame);
        if (re_sampler->read(audio_frame)) {
            return ReadResult::Read;
        }
    }
}
```

**Why:** The current implementation has O(N) latency on the first call where N is the entire source length. For a 3-minute song at 48kHz stereo, that's ~8.6 million frames processed in one audio callback invocation, causing a massive buffer underrun. The fix processes frames incrementally until the resampler has enough to produce one output frame.

---

### 4. Vector Reallocation in Audio Callback (Medium-High)

**Problem:** `AudioMixer::read()` modifies `std::vector<AudioSource> sources` (push_back, erase) which can allocate memory - forbidden on real-time audio threads.

**Fix - use a pre-allocated fixed-size array or a lock-free approach:**
```cpp
class AudioMixer : public AudioSource {
private:
    // Replace std::vector with fixed-capacity container
    static constexpr size_t MAX_SOURCES = 64;
    std::array<std::shared_ptr<AudioSource>, MAX_SOURCES> sources{};
    size_t source_count = 0;

    // Or: use a second lock-free queue for the "active list" swap
};
```

**Alternative (double-buffer approach):**
```cpp
// Producer thread builds new source list
// Audio thread atomically swaps pointer to active list
std::atomic<std::vector<std::shared_ptr<AudioSource>>*> active_sources;
```

**Why:** `std::vector::push_back` can trigger heap allocation (calls `malloc`). `malloc` can take a mutex internally, causing priority inversion on the audio thread. This manifests as occasional audio glitches under load.

---

### 5. Silence Fill Bug in write_frames() (Medium)

**Problem:** The silence-filling code iterates `p_bytes_per_frame` per sample, but `p_bytes_per_frame` already includes all channels.

**Current (lowl_audio_device.cpp:76-88):**
```cpp
unsigned long missing_samples = missing_frames * (unsigned long) audio_source->get_channel_num();
for (; current_sample < missing_samples; current_sample++) {
    for (unsigned long frame_byte = 0; frame_byte < p_bytes_per_frame; frame_byte++) {
        *remaining++ = 0;  // writes bytes_per_frame for EACH sample
    }
}
```

If `p_bytes_per_frame = 8` (2 channels * 4 bytes float) and there are 2 channels, this writes `2 * 8 = 16` zero bytes per frame instead of `8`. It writes double the needed silence.

**Fix:**
```cpp
// Simply zero the remaining buffer
size_t remaining_bytes = missing_frames * p_bytes_per_frame;
std::memset(remaining, 0, remaining_bytes);
```

**Why:** The current code writes `channels * bytes_per_frame` bytes per frame, but `bytes_per_frame` already accounts for channel count. This causes a buffer overrun, writing past the end of the audio buffer, which is undefined behavior.

---

### 6. 24-bit Sample Conversion Is Wrong (Medium)

**Problem:** `sample_to_int24` uses `& 0xFFFFFF` mask after `lround(p_sample * 0x7FFFFF)`, and `write_sample` for INT_24 writes bytes starting from bit 8, skipping the low byte.

**Current (lowl_audio_sample_converter.h:88-94):**
```cpp
static _INLINE_ int32_t sample_to_int24(Sample p_sample) {
    return lround(p_sample * 0x7FFFFF) & 0xFFFFFF;
}

// write_sample for INT_24:
*dst++ = static_cast<uint8_t>(sample >> 8);   // byte 1
*dst++ = static_cast<uint8_t>(sample >> 16);  // byte 2
*dst++ = static_cast<uint8_t>(sample >> 24);  // byte 3 (always 0 after mask!)
```

The mask `& 0xFFFFFF` ensures the value fits in 24 bits, but then the write skips bit 0-7 and writes bits 8-31. Byte 3 (`>> 24`) is always 0 after masking to 24 bits. The actual sample data in bits 0-7 is lost.

**Fix:**
```cpp
static _INLINE_ int32_t sample_to_int24(Sample p_sample) {
    return static_cast<int32_t>(lround(p_sample * 0x7FFFFF));
}

// In write_sample for INT_24 (little-endian):
*dst++ = static_cast<uint8_t>(sample & 0xFF);         // low byte
*dst++ = static_cast<uint8_t>((sample >> 8) & 0xFF);  // mid byte
*dst++ = static_cast<uint8_t>((sample >> 16) & 0xFF); // high byte
```

**Why:** 24-bit audio is common in professional audio interfaces. The current conversion produces corrupted output (effectively 16-bit data shifted up 8 bits with zero low byte), which sounds like quieter, noisier audio than intended.

---

### 7. read_frames() Copies Large Vectors by Value (Medium)

**Problem:** Multiple functions pass `std::vector<float>` and `std::vector<AudioFrame>` by value, causing unnecessary copies of potentially millions of samples.

**Current (lowl_audio_reader.h:43-48):**
```cpp
virtual std::vector<AudioFrame>
read_frames(AudioChannel p_channel, std::vector<float> samples, Error &error);
//                                  ^^^^^^^^^^^^^^^^^^^ copied by value
```

**Fix:**
```cpp
virtual std::vector<AudioFrame>
read_frames(AudioChannel p_channel, std::vector<float> &&samples, Error &error);
// Or: const std::vector<float> &samples if not consuming
```

Similarly for `ChannelConverter::convert()` which takes `std::vector<AudioFrame> audio_data` by value.

**Why:** A 3-minute stereo WAV at 44.1kHz is ~15.8 million floats (~60MB). Passing by value copies this entire buffer. Using move semantics or const reference eliminates the copy.

---

### 8. AudioData Constructor Copies Frame Vector (Low-Medium)

**Problem:** `AudioData` constructor takes `std::vector<AudioFrame>` by value and then moves. The caller at `AudioReader::read_frames` returns by value (eligible for NRVO), so this *sometimes* works, but explicit `std::move` at call sites would be safer.

**Current (lowl_audio_data.h:67):**
```cpp
AudioData(std::vector<AudioFrame> p_audio_frames, SampleRate, AudioChannel);
```

**In practice, callers like `ChannelConverter::convert()` do:**
```cpp
std::unique_ptr<AudioData> audio_data = std::make_unique<AudioData>(
    frames,  // copied! should be std::move(frames)
    p_audio_data->get_sample_rate(), p_to);
```

**Fix:** Ensure all call sites use `std::move(frames)` or change the constructor to take `std::vector<AudioFrame>&&`.

---

### 9. Panning Implementation Is Incorrect for Mono (Low-Medium)

**Problem:** `process_panning()` applies left/right panning to a mono source, which doesn't make physical sense. A mono source should be "placed" in the stereo field, not have its single channel split.

More importantly, the constant-power panning formula `sqrt(1 - pan)` / `sqrt(1 + pan)` treats pan as [-1, +1] but when pan = 0 (center), left gets `sqrt(1) = 1.0` and right gets `sqrt(1) = 1.0`. When pan = 1 (hard right), left gets `sqrt(0) = 0` and right gets `sqrt(2) = 1.414`. This means hard-panned signals are louder than centered signals.

**Fix (equal-power panning):**
```cpp
void process_panning(AudioFrame &audio_frame) {
    Panning pan = panning.load();
    // Map [-1, 1] to [0, pi/2] for equal-power
    Sample angle = static_cast<Sample>((pan + 1.0) * 0.25 * M_PI);
    audio_frame.left *= static_cast<Sample>(std::cos(angle));
    audio_frame.right *= static_cast<Sample>(std::sin(angle));
}
```

**Why:** The current formula increases total energy as panning moves from center. With the equal-power (cos/sin) law, total power remains constant regardless of pan position.

---

### 10. Static Initialization Order Fiasco Risk (Low)

**Problem:** `Lib::drivers` and `Lib::initialized` are static members initialized in `lowl.cpp`. If another translation unit's static initialization calls `Lib::get_drivers()` before `lowl.cpp`'s statics are initialized, you get undefined behavior.

**Fix - use function-local statics (Meyers' singleton):**
```cpp
std::vector<std::shared_ptr<AudioDriver>>& Lib::get_drivers_internal() {
    static std::vector<std::shared_ptr<AudioDriver>> drivers;
    return drivers;
}
```

**Why:** C++ guarantees function-local statics are initialized on first use (thread-safe since C++11), avoiding the static initialization order fiasco entirely.

---

### 11. Missing `noexcept` on Move-Friendly Types (Low)

**Problem:** `AudioFrame` has user-defined copy/move constructors but no `noexcept` specifiers. `std::vector` will use copy instead of move during reallocation if the move constructor isn't `noexcept`.

**Fix:**
```cpp
_INLINE_ AudioFrame(AudioFrame &&p_frame) noexcept = default;
_INLINE_ AudioFrame &operator=(AudioFrame &&p_frame) noexcept = default;
```

Or better, remove the user-defined copy constructor entirely (it's identical to the compiler-generated one), letting the compiler generate all special members as `noexcept` trivial operations.

**Why:** `AudioFrame` is stored in `std::vector` throughout the codebase. Without `noexcept` move, every vector resize copies instead of moves, roughly doubling reallocation cost.

---

### 12. SpaceId Overflow (Low)

**Problem:** `SpaceId` is `uint16_t` (max 65535). `current_id` only increments, never recycles. After 65535 `add_audio()` calls (across `clear_all_audio()` cycles), `current_id` wraps to 0, causing ID collisions and data corruption.

**Fix:** Either recycle IDs (free list) or use `uint32_t` / detect overflow:
```cpp
SpaceId insert_audio_data(std::shared_ptr<AudioData> p_audio_data) {
    if (current_id == std::numeric_limits<SpaceId>::max()) {
        // handle overflow - compact, recycle, or error
    }
    // ...
}
```

---

## Summary Table

| Issue | Severity | Category |
|-------|----------|----------|
| ChannelConverter Error by-value | Critical | Bug |
| Silence fill buffer overrun | Critical | Bug |
| 24-bit sample conversion wrong | Medium | Bug |
| ReSamplerSource drains all frames | High | Performance/Correctness |
| Mixer has no clipping protection | High | Audio Quality |
| Vector realloc in audio callback | Medium-High | Real-time Safety |
| read_frames copies vectors by value | Medium | Performance |
| Panning formula energy increase | Low-Medium | Audio Quality |
| ErrorCode sign inconsistency | Low-Medium | Bug |
| AudioFrame hard-coded stereo | Architectural | Design Limitation |
| Static initialization order risk | Low | Correctness |
| SpaceId overflow | Low | Robustness |
