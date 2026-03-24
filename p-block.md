# Plan: Migrate to Block-Based Processing with Planar AudioBuffer

> Goal: Replace the frame-by-frame `read(AudioFrame&)` pipeline with block-based
> processing using a planar `AudioBuffer`. Interleaved at I/O boundaries, planar
> internally for SIMD-friendly processing. Once complete, multi-channel support
> (Issue #6) becomes a parameter change rather than a structural rewrite.

---

## Current Architecture

```
CoreAudio callback (256-1024 frames)
  -> AudioDevice::write_frames()          // loop: calls read() per frame
       -> AudioSource::read(AudioFrame&)  // virtual, once per sample
            -> AudioData::read()          // copy 1 frame, apply volume/pan
            -> AudioMixer::read()         // sum N sources, normalize, volume/pan
            -> AudioStream::read()        // dequeue 1 frame from SPSC queue
            -> AudioSpace::read()         // delegates to mixer
            -> ReSamplerSource::read()    // drains entire source (bug #4)
```

**Problem:** 48kHz stereo = 48,000 virtual calls/sec/source. The actual arithmetic
is two multiplies per frame. Everything else is overhead.

---

## Target Architecture

```
CoreAudio callback (256-1024 frames)
  -> AudioDevice::write_block()
       -> AudioSource::read_block(AudioBuffer&)      // virtual, once per block
            -> AudioData::read_block()                // memcpy into planar buffer
            -> AudioMixer::read_block()               // sum planar channels
            -> AudioStream::read_block()              // bulk dequeue
            -> AudioSpace::read_block()               // delegates to mixer
            -> ReSamplerSource::read_block()           // feed/drain in chunks
       -> AudioBuffer::interleave(dst)                // planar -> interleaved for hardware
```

### Data flow through a typical block

```
AudioData (internal: std::vector<AudioFrame>)
  -> deinterleave into AudioBuffer        [L0L1L2...] [R0R1R2...]
     -> process_volume_block()            per-channel tight loop (SIMD)
     -> process_panning_block()           per-channel gain (SIMD)
  -> return AudioBuffer to mixer

AudioMixer
  -> for each source: read_block into scratch AudioBuffer
  -> sum scratch into output AudioBuffer   per-channel add loop (SIMD)
  -> normalize_block()                     per-channel peak scan (SIMD)
  -> process_volume_block()
  -> process_panning_block()
  -> return AudioBuffer to device

AudioDevice
  -> AudioBuffer::interleave(hw_buffer)   planar -> interleaved for CoreAudio/WASAPI
```

---

## AudioBuffer: The Core Abstraction

The central new type. Holds planar (deinterleaved) sample data with pre-allocated
storage. All processing operates on this type.

```cpp
class AudioBuffer {
public:
    static constexpr size_t MAX_CHANNELS = 8;  // supports up to 7.1

private:
    std::array<Sample *, MAX_CHANNELS> channel_ptrs{};  // pointers into storage
    Sample *storage;          // pre-allocated contiguous memory, owned or external
    size_t frame_capacity;    // max frames this buffer can hold
    size_t frame_count;       // frames currently valid
    uint8_t channels;         // active channel count

public:
    // Construction -- allocates storage for capacity * channels samples
    AudioBuffer(size_t p_frame_capacity, uint8_t p_channels);

    // Channel access -- returns contiguous Sample* for one channel
    Sample *channel(uint8_t ch)             { return channel_ptrs[ch]; }
    const Sample *channel(uint8_t ch) const { return channel_ptrs[ch]; }

    // Metadata
    size_t get_frame_count() const     { return frame_count; }
    size_t get_frame_capacity() const  { return frame_capacity; }
    uint8_t get_channels() const       { return channels; }
    void set_frame_count(size_t n)     { frame_count = n; }

    // Convert from interleaved source into planar layout
    void deinterleave(const Sample *p_interleaved, size_t p_frame_count);

    // Convert from planar layout into interleaved destination
    void interleave(Sample *p_dst) const;

    // Zero all channels
    void clear();

    // Add another buffer's contents (for mixing)
    void add(const AudioBuffer &other, size_t p_frame_count);
};
```

### Memory layout

One contiguous allocation, sliced into per-channel pointers:

```
storage: [L0 L1 L2 ... L511 | R0 R1 R2 ... R511 | SL0 ... | SR0 ...]
           ^channel_ptrs[0]    ^channel_ptrs[1]    ^[2]       ^[3]

Total: frame_capacity * channels * sizeof(Sample)
512 frames * 2ch * 4 bytes = 4 KB  (stereo)
512 frames * 8ch * 4 bytes = 16 KB (7.1)
```

### Why a class, not a raw Sample**

- Owns its storage -- RAII, no manual lifetime management
- `deinterleave()` / `interleave()` encapsulate the conversion
- `add()` encapsulates mixing
- `clear()` encapsulates zeroing
- Channel count is carried with the data, not passed separately everywhere
- Can be pre-allocated once and reused across callbacks (no heap alloc on RT thread)

### Deinterleave / interleave implementation

These are the only two places that touch interleaved layout. Everything else
is pure planar.

```cpp
void AudioBuffer::deinterleave(const Sample *p_interleaved, size_t p_frames) {
    frame_count = p_frames;
    for (size_t i = 0; i < p_frames; i++) {
        for (uint8_t ch = 0; ch < channels; ch++) {
            channel_ptrs[ch][i] = p_interleaved[i * channels + ch];
        }
    }
}

void AudioBuffer::interleave(Sample *p_dst) const {
    for (size_t i = 0; i < frame_count; i++) {
        for (uint8_t ch = 0; ch < channels; ch++) {
            p_dst[i * channels + ch] = channel_ptrs[ch][i];
        }
    }
}
```

### Add (mixing)

```cpp
void AudioBuffer::add(const AudioBuffer &other, size_t p_frames) {
    for (uint8_t ch = 0; ch < channels; ch++) {
        Sample *dst = channel_ptrs[ch];
        const Sample *src = other.channel_ptrs[ch];
        for (size_t i = 0; i < p_frames; i++) {
            dst[i] += src[i];               // auto-vectorizable
        }
    }
}
```

This is the SIMD payoff. Each inner loop is a contiguous single-type linear
sweep -- exactly what AVX2/NEON are designed for. No stride, no gaps, no
interleaved channels to skip over.

---

## Block Sizing and Buffer Strategy

### Block size

| Block size (frames) | Virtual calls/sec (48kHz) | Latency | Notes |
|---------------------|--------------------------|---------|-------|
| 32 | 1,500 | ~0.7ms | Too small. SIMD overhead dominates. |
| 64 | 750 | ~1.3ms | Hard floor. |
| 128 | 375 | ~2.7ms | Minimum practical. |
| 256 | 187 | ~5.3ms | Good for low-latency. |
| **512** | **94** | **~10.7ms** | **Default.** |
| 1024 | 47 | ~21.3ms | Max typical callback size. |

```cpp
static constexpr size_t MIN_BLOCK_SIZE = 64;
static constexpr size_t DEFAULT_BLOCK_SIZE = 512;
```

Match or divide evenly into the hardware callback size. Use the backend's
preferred buffer size when available, clamped to `[MIN_BLOCK_SIZE, 1024]`.

### Ring buffer depth (block count) -- AudioStream only

```
latency = block_count × block_size / sample_rate
```

At 512 frames/block, 48kHz:

| Blocks | Latency | Use case |
|--------|---------|----------|
| 2 | ~21ms | Double buffer. No jitter margin. |
| **3** | **~32ms** | **Default for internal sources.** |
| 4-6 | ~43-64ms | AudioStream (external producer). |
| 8+ | ~85ms+ | Streaming from disk. |

### Pre-allocated buffers (no heap alloc on RT thread)

Every component pre-allocates its AudioBuffer(s) at construction:

| Component | Buffers | Size |
|-----------|---------|------|
| AudioDevice | 1 output AudioBuffer + 1 interleaved hw buffer | block_size * channels |
| AudioMixer | 1 output AudioBuffer + 1 scratch AudioBuffer | block_size * channels each |
| AudioData | none (writes directly into caller's AudioBuffer) | -- |
| AudioStream | ring buffer of N AudioBuffers | block_count * block_size * channels |
| AudioSpace | none (delegates to mixer) | -- |

---

## Migration Strategy: Dual Interface

Add `read_block(AudioBuffer&)` alongside `read(AudioFrame&)`. Default
implementation bridges old to new. Migrate subclasses one at a time.
Retire `read` when all subclasses are migrated.

The project compiles and works at every intermediate step.

---

## Phase 1: AudioBuffer and Block Interface

### 1.1 Create AudioBuffer

New file: `lowl_audio_buffer.h`, `lowl_audio_buffer.cpp`

Implement the class as described above: planar storage, `deinterleave()`,
`interleave()`, `add()`, `clear()`, channel access.

### 1.2 Add `read_block` to AudioSource

```cpp
// lowl_audio_source.h
virtual BlockReadResult read_block(AudioBuffer &p_buffer);
```

```cpp
struct BlockReadResult {
    size_t frames_read;
    BlockResult status;
};

enum class BlockResult {
    HasData = 0,
    Pause = 1,
    End = 2,
    Remove = 3,
};
```

Non-pure virtual with a default bridge that calls `read()` in a loop,
deinterleaving into the AudioBuffer:

```cpp
// lowl_audio_source.cpp -- default bridge
BlockReadResult AudioSource::read_block(AudioBuffer &p_buffer) {
    AudioFrame frame{};
    size_t written = 0;
    BlockResult status = BlockResult::HasData;

    for (size_t i = 0; i < p_buffer.get_frame_capacity(); i++) {
        ReadResult result = read(frame);
        if (result != ReadResult::Read) {
            status = static_cast<BlockResult>(result);
            break;
        }
        for (uint8_t ch = 0; ch < p_buffer.get_channels(); ch++) {
            p_buffer.channel(ch)[i] = frame[ch];
        }
        written++;
    }
    p_buffer.set_frame_count(written);
    return {written, status};
}
```

### 1.3 Add block-based volume/panning helpers

```cpp
// lowl_audio_source.h (protected)
void process_volume_block(AudioBuffer &p_buffer);
void process_panning_block(AudioBuffer &p_buffer);
```

```cpp
// lowl_audio_source.cpp
void AudioSource::process_volume_block(AudioBuffer &p_buffer) {
    const Volume vol = volume.load(std::memory_order_relaxed);
    for (uint8_t ch = 0; ch < p_buffer.get_channels(); ch++) {
        Sample *data = p_buffer.channel(ch);
        size_t count = p_buffer.get_frame_count();
        for (size_t i = 0; i < count; i++) {
            data[i] *= vol;                 // auto-vectorizable
        }
    }
}

void AudioSource::process_panning_block(AudioBuffer &p_buffer) {
    const Panning pan = panning.load(std::memory_order_relaxed);
    const Sample gain_l = static_cast<Sample>(std::sqrt(1.0 - pan));
    const Sample gain_r = static_cast<Sample>(std::sqrt(1.0 + pan));

    // Compute gain once, apply to entire channel -- tight SIMD loop
    if (p_buffer.get_channels() >= 1) {
        Sample *left = p_buffer.channel(0);
        for (size_t i = 0; i < p_buffer.get_frame_count(); i++) {
            left[i] *= gain_l;
        }
    }
    if (p_buffer.get_channels() >= 2) {
        Sample *right = p_buffer.channel(1);
        for (size_t i = 0; i < p_buffer.get_frame_count(); i++) {
            right[i] *= gain_r;
        }
    }
}
```

Compare to current code which computes `std::sqrt()` on every single frame.
Block version computes it once per block, then applies a constant multiply
across 512 samples -- ideal for SIMD.

**Files created:** `lowl_audio_buffer.h`, `lowl_audio_buffer.cpp`
**Files changed:** `lowl_audio_source.h`, `lowl_audio_source.cpp`
**Risk:** None. Additive only.

---

## Phase 2: Wire the Backend

### 2.1 Rewrite AudioDevice::write_frames to use read_block

Pre-allocate an `AudioBuffer` and an interleaved output buffer at `start()`:

```cpp
// lowl_audio_device.h (protected members)
AudioBuffer output_buffer;                // planar, pre-allocated
std::vector<Sample> interleave_buffer;    // interleaved, pre-allocated

void AudioDevice::allocate_buffers(size_t p_max_frames, uint8_t p_channels) {
    output_buffer = AudioBuffer(p_max_frames, p_channels);
    interleave_buffer.resize(p_max_frames * p_channels);
}
```

```cpp
void AudioDevice::write_frames(
    void *p_dst,
    unsigned long p_frames_per_buffer,
    unsigned long p_bytes_per_frame
) const {
    const uint8_t channels = static_cast<uint8_t>(audio_source->get_channel_num());

    // 1. Read into planar AudioBuffer
    output_buffer.clear();
    auto [frames_read, status] = audio_source->read_block(output_buffer);

    // 2. Interleave into flat buffer
    output_buffer.interleave(interleave_buffer.data());

    // 3. Convert to device sample format and write to hardware
    auto *dst = static_cast<uint8_t *>(p_dst);
    for (size_t i = 0; i < frames_read * channels; i++) {
        Sample sample = std::clamp(
            interleave_buffer[i],
            AudioFrame::MIN_SAMPLE_VALUE,
            AudioFrame::MAX_SAMPLE_VALUE
        );
        void *write_ptr = dst;
        SampleConverter::write_sample(
            audio_device_properties.sample_format, sample, &write_ptr
        );
        dst += SampleConverter::get_sample_size(audio_device_properties.sample_format);
    }

    // 4. Silence-fill remainder
    if (frames_read < p_frames_per_buffer) {
        unsigned long missing = p_frames_per_buffer - frames_read;
        std::memset(dst, 0, missing * p_bytes_per_frame);
    }
}
```

**Files changed:** `lowl_audio_device.h`, `lowl_audio_device.cpp`
**Risk:** Low. CoreAudio/WASAPI callbacks already pass buffer + frame_count.

---

## Phase 3: Migrate Subclasses (one at a time)

Each subclass overrides `read_block(AudioBuffer&)` with a native implementation.
The old `read()` stays temporarily. Order by impact:

### 3.1 AudioData (easiest, highest payoff)

Current: copies one AudioFrame at a time.

Native version: deinterleave from internal `std::vector<AudioFrame>` into the
caller's AudioBuffer, then apply volume/panning as planar block ops.

```cpp
BlockReadResult AudioData::read_block(AudioBuffer &p_buffer) {
    if (!is_playing) return {0, BlockResult::Pause};
    if (!is_not_reset.test_and_set()) {
        position = seek_position.load();
        seek_position = 0;
    }
    size_t available = (position < size) ? (size - position) : 0;
    size_t to_read = std::min(p_buffer.get_frame_capacity(), available);
    if (to_read == 0) {
        position = 0;
        return {0, BlockResult::Remove};
    }

    // Deinterleave from AudioFrame storage into planar buffer
    uint8_t channels = p_buffer.get_channels();
    for (size_t i = 0; i < to_read; i++) {
        const AudioFrame &f = frames[position + i];
        for (uint8_t ch = 0; ch < channels; ch++) {
            p_buffer.channel(ch)[i] = f[ch];
        }
    }
    p_buffer.set_frame_count(to_read);
    position += to_read;

    process_volume_block(p_buffer);
    process_panning_block(p_buffer);
    return {to_read, BlockResult::HasData};
}
```

**Note:** Internal storage stays as `std::vector<AudioFrame>` for now.
Phase 5 flattens it to planar `std::vector<Sample>`, at which point the
deinterleave loop becomes a per-channel `memcpy`.

**Files changed:** `lowl_audio_data.h`, `lowl_audio_data.cpp`

### 3.2 AudioMixer (biggest architectural win)

Pre-allocate one scratch `AudioBuffer` at construction. For each source,
`read_block` into scratch, then `add()` into the output.

```cpp
// lowl_audio_mixer.h
AudioBuffer scratch_buffer;  // pre-allocated at construction

AudioMixer::AudioMixer(SampleRate rate, AudioChannel ch,
                       size_t p_block_size = DEFAULT_BLOCK_SIZE)
    : AudioSource(rate, ch),
      scratch_buffer(p_block_size, get_channel_num(ch)) {
    // ...
}
```

```cpp
BlockReadResult AudioMixer::read_block(AudioBuffer &p_buffer) {
    drain_events();
    if (!is_playing) return {0, BlockResult::Pause};

    p_buffer.clear();
    bool has_output = false;
    bool has_removals = false;

    for (size_t s = 0; s < sources.size(); s++) {
        scratch_buffer.clear();
        auto [n, status] = sources[s]->read_block(scratch_buffer);

        if (n > 0) {
            p_buffer.add(scratch_buffer, n);        // per-channel SIMD add
            if (n > p_buffer.get_frame_count()) {
                p_buffer.set_frame_count(n);
            }
            has_output = true;
        }
        if (status == BlockResult::Remove) {
            sources[s] = nullptr;
            has_removals = true;
        }
    }

    if (has_removals) {
        sources.erase(
            std::remove(sources.begin(), sources.end(), nullptr),
            sources.end()
        );
    }

    if (!has_output) return {0, BlockResult::End};

    normalize_block(p_buffer);
    process_volume_block(p_buffer);
    process_panning_block(p_buffer);
    return {p_buffer.get_frame_count(), BlockResult::HasData};
}
```

`normalize_block` operates per-channel -- scan for peak across all channels,
then scale each channel independently. Same logic as current normalizer but
over contiguous arrays:

```cpp
void AudioMixer::normalize_block(AudioBuffer &p_buffer) {
    if (!normalize_output.load(std::memory_order_relaxed)) return;

    // Find global peak across all channels
    Sample peak = 0;
    for (uint8_t ch = 0; ch < p_buffer.get_channels(); ch++) {
        const Sample *data = p_buffer.channel(ch);
        for (size_t i = 0; i < p_buffer.get_frame_count(); i++) {
            peak = std::max(peak, std::abs(data[i]));
        }
    }

    // Scale all channels by the same factor (preserves balance)
    if (peak > AudioFrame::MAX_SAMPLE_VALUE) {
        Sample attenuation = AudioFrame::MAX_SAMPLE_VALUE / peak;
        for (uint8_t ch = 0; ch < p_buffer.get_channels(); ch++) {
            Sample *data = p_buffer.channel(ch);
            for (size_t i = 0; i < p_buffer.get_frame_count(); i++) {
                data[i] *= attenuation;       // auto-vectorizable
            }
        }
    }
}
```

**Files changed:** `lowl_audio_mixer.h`, `lowl_audio_mixer.cpp`

### 3.3 AudioStream

Replace per-frame SPSC queue with a ring buffer of `AudioBuffer`s.

The stream holds N pre-allocated `AudioBuffer`s in a ring. The producer
fills one and advances the write cursor. The consumer reads one and
advances the read cursor.

```cpp
// lowl_audio_stream.h
static constexpr size_t DEFAULT_BLOCK_COUNT = 6;
std::vector<AudioBuffer> ring;             // N pre-allocated planar buffers
std::atomic<size_t> read_pos{0};
std::atomic<size_t> write_pos{0};
size_t ring_capacity;                      // number of AudioBuffers in ring

AudioStream::AudioStream(SampleRate rate, AudioChannel ch,
                         size_t p_block_size = DEFAULT_BLOCK_SIZE,
                         size_t p_block_count = DEFAULT_BLOCK_COUNT)
    : AudioSource(rate, ch), ring_capacity(p_block_count) {
    uint8_t channels = get_channel_num(ch);
    ring.reserve(p_block_count);
    for (size_t i = 0; i < p_block_count; i++) {
        ring.emplace_back(p_block_size, channels);
    }
}
```

Producer writes interleaved data in, it gets deinterleaved into the next
ring slot:

```cpp
bool AudioStream::write(const Sample *p_interleaved, size_t p_frame_count) {
    size_t wp = write_pos.load(std::memory_order_relaxed);
    size_t rp = read_pos.load(std::memory_order_acquire);
    size_t next = (wp + 1) % ring_capacity;
    if (next == rp) return false;  // ring full

    ring[wp].deinterleave(p_interleaved, p_frame_count);
    write_pos.store(next, std::memory_order_release);
    return true;
}
```

Consumer reads planar data directly -- zero-copy from ring slot into caller:

```cpp
BlockReadResult AudioStream::read_block(AudioBuffer &p_buffer) {
    if (!is_playing) return {0, BlockResult::Pause};
    size_t rp = read_pos.load(std::memory_order_relaxed);
    size_t wp = write_pos.load(std::memory_order_acquire);
    if (rp == wp) return {0, BlockResult::End};  // ring empty

    const AudioBuffer &src = ring[rp];
    size_t frames = src.get_frame_count();

    // Copy planar channel data
    for (uint8_t ch = 0; ch < p_buffer.get_channels(); ch++) {
        std::memcpy(p_buffer.channel(ch), src.channel(ch), frames * sizeof(Sample));
    }
    p_buffer.set_frame_count(frames);

    read_pos.store((rp + 1) % ring_capacity, std::memory_order_release);

    process_volume_block(p_buffer);
    process_panning_block(p_buffer);
    return {frames, BlockResult::HasData};
}
```

**Files changed:** `lowl_audio_stream.h`, `lowl_audio_stream.cpp`

### 3.4 AudioSpace

Trivial -- forward to mixer, apply volume/panning on the result.

```cpp
BlockReadResult AudioSpace::read_block(AudioBuffer &p_buffer) {
    auto result = mixer->read_block(p_buffer);
    if (result.frames_read > 0) {
        process_volume_block(p_buffer);
        process_panning_block(p_buffer);
    }
    return result;
}
```

**Files changed:** `lowl_audio_space.h`, `lowl_audio_space.cpp`

### 3.5 ReSamplerSource

Block-based version naturally fixes Bug #4:

1. Check if resampler has enough output -- drain into AudioBuffer
2. If not, read a block from upstream source into scratch AudioBuffer
3. Interleave scratch (resampler expects interleaved), feed to resampler
4. Repeat until output is full or source exhausted

The resampler (r8brain) works on interleaved data internally, so this is
one of the few places where interleave/deinterleave happens mid-pipeline.
That's fine -- it's contained within this one source type.

**Files changed:** `lowl_audio_re_sampler_source.h`, `lowl_audio_re_sampler_source.cpp`
**Bonus:** Fixes Issue #4 as a side effect.

---

## Phase 4: Retire Frame-by-Frame

Once all subclasses implement `read_block(AudioBuffer&)` natively:

1. Remove the default bridge implementation from `AudioSource::read_block`
2. Make `read_block(AudioBuffer&)` pure virtual
3. Remove `read(AudioFrame&)` from `AudioSource`
4. Remove `process_volume(AudioFrame&)` and `process_panning(AudioFrame&)`
5. `AudioFrame` struct remains for reader output and converter intermediates
   but is no longer part of the `AudioSource` interface

**This is the only breaking change.** Everything before this compiles incrementally.

---

## Phase 5: Flatten Internal Storage (Prepares for Issue #6)

Once the pipeline operates on `AudioBuffer` end-to-end:

- **AudioData:** Store `AudioBuffer` (planar) instead of `std::vector<AudioFrame>`.
  Readers decode directly into planar storage. `read_block` becomes per-channel
  `memcpy` -- no deinterleave step.
- **Readers:** Decode into `AudioBuffer` instead of building `std::vector<AudioFrame>`.
  Most codecs (MP3, FLAC, Opus, OGG) produce interleaved output, so readers call
  `AudioBuffer::deinterleave()` once at load time.
- **AudioFrame:** Can be removed entirely once no code references it.

At this point, channel count is just a constructor argument to `AudioBuffer`.
Multi-channel (Issue #6) is changing `channels = 4` instead of `2`.
No structural changes needed.

---

## Phase 6: Multi-Channel (Issue #6)

With AudioBuffer in place, multi-channel support is straightforward:

1. Keep `AudioChannel::Quadraphonic` and wider `AudioChannelMask` values
   (Solution D from ISSUE-6.md)
2. Remove the `supports_audio_frame_channel()` guard -- AudioBuffer handles
   any channel count up to `MAX_CHANNELS`
3. Update `process_panning_block` with proper multi-channel panning laws
4. Update readers/converters to populate >2 channels
5. Backend device negotiation can now accept wider channel configs

No pipeline changes. No new types. Just wider data flowing through the
same AudioBuffer abstraction.

---

## Dependency Map

```
Phase 1 (AudioBuffer + interface) -- no deps, additive only
Phase 2 (backend)                 -- depends on Phase 1
Phase 3.1 (AudioData)             -- depends on Phase 1
Phase 3.2 (AudioMixer)            -- depends on Phase 1
Phase 3.3 (AudioStream)           -- depends on Phase 1
Phase 3.4 (AudioSpace)            -- depends on Phase 3.2
Phase 3.5 (ReSampler)             -- depends on Phase 1
Phase 4 (retire read)             -- depends on all of Phase 3
Phase 5 (flatten storage)         -- depends on Phase 4
Phase 6 (multi-channel)           -- depends on Phase 5
```

Phases 2, 3.1, 3.2, 3.3, and 3.5 can be done in parallel after Phase 1.

---

## Issues Addressed by This Migration

| Issue | How |
|-------|-----|
| #4 ReSamplerSource drains entire source | Block-based incremental feed/drain |
| #6 AudioFrame hard-coded stereo | AudioBuffer supports N channels natively |
| #7 Vector reallocation in mixer callback | Pre-allocated AudioBuffer scratch |
| #9 Vector copy-by-value in readers | Readers write directly into AudioBuffer |

---

## What This Plan Does NOT Cover

- DSP effects pipeline -- builds on top of AudioBuffer naturally (effects
  receive and return AudioBuffer, processing per-channel planar data)
- Explicit SIMD intrinsics -- auto-vectorization from tight per-channel loops
  is the first step; hand-written SIMD can be added per-function later
- Audio bus / group system -- AudioBuffer is the right unit for bus routing
  (sum sources into bus AudioBuffer, apply per-bus effects, sum buses into
  master AudioBuffer)
