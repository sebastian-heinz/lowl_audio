# Ring Buffer Optimization Analysis

Comparison of our `AudioStream` ring buffer (`src/audio/source/lowl_audio_stream.h/.cpp`)
against the techniques described in David Alvarez Rosa's "Optimizing a Lock-Free Ring Buffer" (2026-03-24).

---

## Current State of Our Implementation

Our `AudioStream` is already a **lock-free SPSC ring buffer** with correctly tuned memory ordering:

- `read_position` and `write_position` are `std::atomic<size_t>`
- The producer (write side) loads `write_position` with `relaxed`, loads `read_position` via `get_available_frames_to_read()` with `acquire`, and stores `write_position` with `release`
- The consumer (render side) loads `read_position` with `relaxed`, loads `write_position` with `acquire`, and stores `read_position` with `release`
- Uses monotonically increasing positions with modulo (`%`) for array indexing

This places us roughly at **V4** of the blog post (lock-free + tuned memory order).
The blog reports V4 at ~108M ops/s vs V5 at ~305M ops/s, a 3x improvement from index caching alone.

Our buffer also does **bulk frame copies** (not single-element push/pop), which amortizes per-operation overhead. Still, the optimizations below target the remaining bottlenecks in our hot path.

---

## Optimization 1: Cache-Line Align the Atomics (Prevent False Sharing)

**Problem:** `read_position` and `write_position` are declared as adjacent members in `AudioStream`. On most architectures, they will land in the same 64-byte cache line. When the producer stores `write_position` and the consumer stores `read_position`, the CPU bounces that shared cache line between cores -- this is *false sharing*.

**What the blog does:** Uses `alignas(std::hardware_destructive_interference_size)` on each atomic to force them into separate cache lines.

**Concrete change in `lowl_audio_stream.h`:**

```cpp
// BEFORE
std::atomic<size_t> read_position{0};
std::atomic<size_t> write_position{0};

// AFTER
static constexpr size_t CacheLineSize = 64; // std::hardware_destructive_interference_size
alignas(CacheLineSize) std::atomic<size_t> read_position{0};
alignas(CacheLineSize) std::atomic<size_t> write_position{0};
```

**Impact:** Eliminates false sharing. On the blog's benchmark this alone contributed to the jump from V3 (35M) to V4 (108M). In our case, since we do bulk copies, the effect per `render()`/`write()` call is smaller, but on high-frequency small-block renders (e.g., 64-frame blocks at 48kHz) this matters.

---

## Optimization 2: Cached Remote Index (The V5 Technique)

**Problem:** Every call to `get_available_frames_to_read()` and `get_available_frames_to_write()` loads the *remote* atomic (`acquire`), which forces a cache-line fetch from the other core. In steady state, the answer rarely changes between consecutive calls.

**What the blog does:** Each side keeps a local (non-atomic) cached copy of the other side's index. It only reloads the remote atomic when the cached value says the buffer is full/empty.

**Concrete change in `lowl_audio_stream.h`:**

```cpp
// Add cached copies, each on their own cache line to stay with their owner
alignas(CacheLineSize) std::atomic<size_t> read_position{0};
alignas(CacheLineSize) size_t cached_read_position{0};   // written only by producer
alignas(CacheLineSize) std::atomic<size_t> write_position{0};
alignas(CacheLineSize) size_t cached_write_position{0};  // written only by consumer
```

**Concrete change in `write_interleaved()` / `write_planar()`:**

```cpp
size_l AudioStream::write_interleaved(const Sample *p_interleaved, const size_t p_frame_count) {
    const size_t current_write = write_position.load(std::memory_order_relaxed);

    // Use cached read position first; only reload if it looks full
    size_t available = frame_capacity - (current_write - cached_read_position);
    if (available == 0) {
        cached_read_position = read_position.load(std::memory_order_acquire);
        available = frame_capacity - (current_write - cached_read_position);
    }

    const size_t writable_frames = std::min<size_t>(available, p_frame_count);
    if (writable_frames == 0) {
        return 0;
    }
    copy_interleaved_to_ring(p_interleaved, writable_frames, current_write);
    write_position.store(current_write + writable_frames, std::memory_order_release);
    return writable_frames;
}
```

**Concrete change in `render()`:**

```cpp
RenderResult AudioStream::render(AudioBlockView p_block) {
    // ... playback_enabled / channel check ...

    const size_t current_read = read_position.load(std::memory_order_relaxed);

    // Use cached write position first; only reload if it looks empty
    size_t available = cached_write_position - current_read;
    if (available == 0) {
        cached_write_position = write_position.load(std::memory_order_acquire);
        available = cached_write_position - current_read;
    }

    const uint32_t frames_to_read = static_cast<uint32_t>(std::min<size_t>(available, p_block.frame_count));
    if (frames_to_read == 0) {
        return {0, RenderState::Starved};
    }

    copy_from_ring(p_block, frames_to_read, current_read);
    read_position.store(current_read + frames_to_read, std::memory_order_release);
    // ... volume/panning ...
    return {frames_to_read, RenderState::Ok};
}
```

**Impact:** The blog measured a 3x throughput improvement (108M -> 305M ops/s) from this single change. In audio, the render callback runs at a fixed interval, so the primary benefit is *lower latency per call* and fewer cache misses on the audio thread (which must never stall).

---

## Optimization 3: Power-of-Two Capacity with Bitmask Wrap-Around

**Problem:** Our `copy_from_ring` and `copy_interleaved_to_ring` compute the ring index with `p_write_position % frame_capacity`. This has two problems:

1. **Performance:** The `%` operator compiles to a division instruction on non-power-of-two sizes, which is significantly slower than a bitwise AND.
2. **Correctness (Issue 48):** On 32-bit platforms (`size_t` = 32 bits), monotonically increasing positions wrap after `2^32 / 48000 ≈ 24.8 hours` at 48kHz. When `size_t` overflows from `SIZE_MAX` to `0`, the modulo result jumps discontinuously -- `SIZE_MAX % frame_capacity` is some arbitrary value, then suddenly `0 % frame_capacity = 0`. This corrupts the ring index mapping and causes audio glitches or reads from wrong positions.

**What the blog does:** Constrains capacity to a power of two, then uses `index & (capacity - 1)` instead of `index % capacity`.

**Why this also fixes Issue 48:** With a power-of-two capacity, `position & capacity_mask` only examines the lower N bits of the position. When `size_t` wraps from `0xFFFFFFFF` to `0x00000000`, the lower bits simply continue their natural sequence -- the bitmask is immune to unsigned overflow. Similarly, the available-frames calculation `write_position - read_position` is well-defined unsigned subtraction and produces the correct distance even across the wrap boundary. Together, these two properties make the ring buffer correct for arbitrarily long runtime on any word size.

**Concrete change in `lowl_audio_stream.h`:**

```cpp
size_t frame_capacity = 0;
size_t capacity_mask = 0;    // NEW: frame_capacity - 1, for bitmask wrap
```

**Concrete change in the constructor:**

```cpp
AudioStream::AudioStream(SampleRate p_sample_rate, ChannelLayout p_channel_layout, size_t size)
    : AudioSource(p_sample_rate, p_channel_layout) {
    // Round up to next power of two
    size_t capacity = 1;
    while (capacity < size) {
        capacity <<= 1;
    }
    frame_capacity = capacity;
    capacity_mask = capacity - 1;
    const uint8_t channel_count = get_channel_count();
    channels = std::vector<std::vector<Sample>>(
        channel_count, std::vector<Sample>(frame_capacity, static_cast<Sample>(0)));
}
```

**Concrete change in `copy_from_ring`:**

```cpp
// BEFORE
const size_t first_index = p_read_position % frame_capacity;

// AFTER
const size_t first_index = p_read_position & capacity_mask;
```

Same change applies in `copy_interleaved_to_ring` and `copy_planar_to_ring`.

**Impact:** Replaces a ~20-40 cycle `div` instruction with a 1-cycle `and` on every ring buffer access. The blog mentions this as a known further optimization. On ARM (Android, iOS) where division is even more expensive, this matters more. Additionally, this is the **only** change needed to fix Issue 48 -- no `static_assert`, no position rebasing, no 64-bit requirement.

**Trade-off:** Uses up to 2x the requested memory. For our default of 375,000 frames, we'd round to 524,288 frames (~4.2 MB stereo float instead of ~3 MB). Acceptable for desktop/console; may need care on memory-constrained mobile.

---

## Optimization 4: Batch-Aware Prefetching

**Problem:** Our `copy_from_ring` copies frames sequentially from the ring into the output block. When the read position wraps around, the second chunk (`second_part_frames`) starts at the beginning of the array, which may be cold in the CPU cache -- especially if the buffer is large (3+ MB).

**Concrete change in `copy_from_ring`:**

```cpp
void AudioStream::copy_from_ring(AudioBlockView p_block,
                                 const uint32_t p_frames_to_read,
                                 const size_t p_read_position) {
    if (frame_capacity == 0 || p_frames_to_read == 0) {
        return;
    }

    const size_t first_index = p_read_position & capacity_mask;
    const uint32_t first_part_frames =
        static_cast<uint32_t>(std::min<size_t>(p_frames_to_read, frame_capacity - first_index));
    const uint32_t second_part_frames = p_frames_to_read - first_part_frames;

    // Prefetch the wrap-around region while copying the first part
    if (second_part_frames > 0) {
        for (uint8_t ch = 0; ch < p_block.channel_count; ch++) {
            __builtin_prefetch(channels[ch].data(), 0 /* read */, 1 /* low temporal locality */);
        }
    }

    for (uint8_t channel_index = 0; channel_index < p_block.channel_count; channel_index++) {
        Sample *dst = p_block.channel(channel_index);
        const std::vector<Sample> &src_channel = channels[static_cast<size_t>(channel_index)];
        std::copy_n(src_channel.data() + first_index, first_part_frames, dst);
        if (second_part_frames > 0) {
            std::copy_n(src_channel.data(), second_part_frames, dst + first_part_frames);
        }
    }
}
```

**Impact:** Hides memory latency for the wrap-around case. The prefetch executes while the first `std::copy_n` is running, so by the time we reach the second copy, the data is already in L1/L2. Most impactful when `frame_capacity` is large (our default is 375K frames = ~3 MB per channel).

**Portability note:** `__builtin_prefetch` is GCC/Clang. For MSVC, use `_mm_prefetch`. Wrap in a macro or `#ifdef` for cross-platform.

---

## Optimization 5: Separate Producer/Consumer Data onto Different Cache Lines (Struct Layout)

**Problem:** Beyond the atomics themselves, the *data accessed alongside them* also causes cache contention. In our class, `channels` (a `std::vector` of vectors) sits between `read_position` and `write_position`. When the consumer reads `channels[i]`, it may pull in the producer's nearby data, and vice versa.

**Concrete change -- reorganize `lowl_audio_stream.h` member layout:**

```cpp
class AudioStream : public AudioSource {
private:
    static constexpr size_t CacheLineSize = 64;

    // --- Shared (read-only after construction) ---
    std::vector<std::vector<Sample>> channels;
    size_t frame_capacity = 0;
    size_t capacity_mask = 0;

    // --- Producer side (write_interleaved / write_planar) ---
    alignas(CacheLineSize) std::atomic<size_t> write_position{0};
    size_t cached_read_position{0};

    // --- Consumer side (render) ---
    alignas(CacheLineSize) std::atomic<size_t> read_position{0};
    size_t cached_write_position{0};
};
```

**Why this layout works:**
- `channels`, `frame_capacity`, `capacity_mask` are written once at construction and then read-only. They can live anywhere without contention.
- The producer's hot fields (`write_position` + its cache of `read_position`) are on one cache line.
- The consumer's hot fields (`read_position` + its cache of `write_position`) are on a separate cache line.
- Neither side's store invalidates the other side's working set.

**Impact:** Complements Optimizations 1 and 2. Without proper layout, even cached indices can land on the same cache line as a remote atomic, re-introducing false sharing through the back door. This is the structural foundation that makes the other optimizations effective.

---

## Summary Table

| # | Optimization | Blog Equivalent | Effort | Expected Impact |
|---|---|---|---|---|
| 1 | Cache-line align atomics | V3 -> V4 | Low | Eliminates false sharing; critical for small block sizes |
| 2 | Cached remote index | V4 -> V5 | Medium | Up to 3x fewer cross-core cache fetches per call |
| 3 | Power-of-two bitmask | Mentioned as known opt | Low | Removes division from every ring access; **fixes Issue 48** (32-bit overflow) |
| 4 | Batch-aware prefetching | Beyond blog scope | Low | Hides wrap-around latency on large buffers |
| 5 | Struct layout separation | Implicit in blog's `alignas` | Low | Foundation for 1+2; prevents accidental re-sharing |

## Recommendation

Optimizations 1, 3, and 5 are low-effort, low-risk changes that should be applied regardless. Optimization 2 (index caching) delivers the largest single improvement and is the recommended next step. Optimization 4 is a refinement that helps specifically with large ring buffers and wrap-around.

All five changes are compatible with each other and with our existing monotonically-increasing position scheme (which is already correct and avoids the blog's "waste one slot" approach).

---

## Issue 48: 32-bit Position Overflow -- Resolved by Optimization 3

**Reference:** `issuev2.md` Issue 48 -- "`AudioStream` ring buffer positions overflow on 32-bit after ~24 hours"

**Root cause:** `read_position` and `write_position` are monotonically increasing `std::atomic<size_t>`. On 32-bit platforms, `size_t` is 32 bits and wraps after `2^32 / 48000 ≈ 24.8 hours` at 48kHz. The current `% frame_capacity` operation produces a discontinuous index when the position wraps from `SIZE_MAX` to `0` (unless `frame_capacity` happens to be a power of two).

**Why Optimization 3 is a complete fix:**

1. **Index mapping** -- `position & capacity_mask` only reads the lower N bits. Unsigned overflow does not affect the lower bits; they continue their natural `0..capacity-1` cycle uninterrupted.
2. **Distance calculation** -- `write_position - read_position` is unsigned subtraction, which is well-defined in C++ and produces the correct modular distance even when `write_position` has wrapped past `read_position`.
3. **Split-copy logic** -- `frame_capacity - first_index` remains valid because `first_index` is always in `[0, capacity)` after the bitmask.

**No additional fix needed.** The `static_assert(sizeof(size_t) >= 8)` or position-rebasing approaches suggested in issuev2.md are unnecessary once the capacity is constrained to a power of two. This makes Optimization 3 a correctness fix as well as a performance improvement.

---

Note: The third-party `readerwriterqueue` library already in this project (`third_party/readerwriterqueue/`) implements many of these techniques. If a general-purpose SPSC queue is ever needed elsewhere, prefer that library. The `AudioStream` ring buffer is purpose-built for bulk audio frame transfers and benefits from domain-specific optimizations (planar/interleaved copy, multi-channel layout) that a generic queue cannot provide.
