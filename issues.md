# lowl_audio — Comprehensive Issue Tracker

## Scope

- Scope is the `lowl_audio` core library (`src/` and `test/`) only.
- Godot wrapper changes are out of scope.
- Issues are merged from the original action plan and a full code review of every source file.

## Current Baseline

- `./build/test/lowl_audio_test --test-case="AudioData"` passes.
- `./build/test/lowl_audio_test --test-case="AudioSpace"` passes.
- `./build/test/lowl_audio_test --test-case="AudioStream"` passes.
- `./build/test/lowl_audio_test` also fails in `Driver`, but that failure is environment-dependent.

---

## Issue Index

| #  | Severity | Category | Status | Title |
|----|----------|----------|--------|-------|
| 1  | Critical | Bug | **Fixed** | INT_24 sample output writes wrong bytes |
| 2  | Critical | Bug | **Fixed** | `Buffer::grow()` OOM silently causes buffer overflow |
| 3  | Critical | Bug | **Fixed** | OGG `SEEK_END` adds to position instead of setting it |
| 4  | Critical | Bug | **Fixed** | `ReleasePool` destructor use-after-free |
| 5  | Critical | Bug | **Fixed** | `sample_to_uint8` undefined behavior on negative samples |
| 6  | High | Bug | **Fixed** | `Buffer::read_u16()` / `read_u32()` unsequenced side effects |
| 7  | High | Semantic | **Fixed** | `AudioVoice` constructor pauses by default, breaking standalone use |
| 8  | High | Semantic | **Fixed** | Reset/seek/stop do not immediately update query state |
| 9  | High | Semantic | Open | Mixer scratch buffer truncates blocks larger than 8192 frames |
| 10 | High | Semantic | Open | `AudioSpace::render()` ignores `playback_enabled` gate |
| 11 | High | Thread | Open | `Lowl::Lib::drivers` data race between `initialize()` and readers |
| 12 | High | Thread | Open | `AudioSource::name` data race between control and render threads |
| 13 | Medium | Thread | Open | `Logger` static state unprotected across threads |
| 14 | Medium | Performance | Open | Mixer linear scan over 1024 slots on the real-time thread |
| 15 | Medium | Lifecycle | Open | Playback slot and asset-id monotonic exhaustion |
| 16 | Medium | Resource | Open | CoreAudio `get_num_channel` memory leak and wrong allocation |
| 17 | Medium | Build | Open | ELF-only linker flags applied on macOS Clang builds |
| 18 | Low | Bug | Open | `format_log()` argument order inconsistency between size pass and format pass |
| 19 | Low | Quality | Open | `Buffer` class: dead code, Rule-of-Five violation, raw `slice()` pointer |
| 20 | Low | Quality | Open | `SampleConverter` C-style casts, misnamed variable, `_INLINE_` misuse |
| 21 | Low | Quality | Open | `AudioDeviceProperties` uninitialized members; `get_closest_properties()` stub |
| 22 | Low | Quality | Open | `Timer` uses raw `new std::thread`; minor resource hygiene |
| 23 | Low | Testing | Open | Driver test conflates platform availability with core regressions |

---

## Issue 1 — INT_24 Sample Output Writes Wrong Bytes — **FIXED**

**Severity:** Critical
**Files:** `src/audio/convert/lowl_audio_sample_converter.h:91-95`

### Problem

`sample_to_int24()` returns a 24-bit value in the low 24 bits of an `int32_t`. But `write_sample()` for `INT_24` discards the least-significant byte and writes a zero byte instead:

```cpp
*dst++ = static_cast<uint8_t>(sample >> 8);   // bits 8-15  (should be bits 0-7)
*dst++ = static_cast<uint8_t>(sample >> 16);  // bits 16-23 (should be bits 8-15)
*dst++ = static_cast<uint8_t>(sample >> 24);  // always 0   (should be bits 16-23)
```

Every INT_24 audio output from a CoreAudio or WASAPI device using this format is silently garbled. The LSB is lost and replaced with zero, reducing effective resolution to roughly 16-bit.

### Solution A — Fix the shift offsets

Change the three writes to `sample >> 0`, `sample >> 8`, `sample >> 16`. This is a one-line conceptual fix (three byte writes). It is the minimal, lowest-risk change.

### Solution B — Rewrite INT_24 to pack via a struct or memcpy

Pack the 24-bit value into a 3-byte little-endian representation using `memcpy` with explicit byte layout. This avoids any ambiguity about shift direction and is more self-documenting.

### Recommendation

**Solution A.** The shift fix is obviously correct, matches the existing pattern used by the INT_24 *reader* path in `lowl_audio_reader.cpp`, and keeps the code structure consistent with the other format cases in `write_sample()`.

```cpp
case SampleFormat::INT_24: {
    int32_t sample = sample_to_int24(p_sample);
    uint8_t *dst = (uint8_t *)*p_dst;
    *dst++ = static_cast<uint8_t>(sample);        // bits 0-7
    *dst++ = static_cast<uint8_t>(sample >> 8);   // bits 8-15
    *dst++ = static_cast<uint8_t>(sample >> 16);  // bits 16-23
    *p_dst = dst;
    break;
}
```

**Fix applied:** Solution A — corrected shift offsets to `>> 0`, `>> 8`, `>> 16`.

---

## Issue 2 — `Buffer::grow()` OOM Silently Causes Buffer Overflow — **FIXED**

**Severity:** Critical
**Files:** `src/lowl_buffer.cpp:139-147`

### Problem

If `realloc` returns `nullptr`, `grow()` silently returns without updating `data` or `real_length`. The caller (`write_data`) proceeds to `memcpy` past the end of the original allocation, corrupting the heap or crashing.

### Solution A — Abort or throw on allocation failure

Treat OOM as fatal: call `std::abort()` or throw `std::bad_alloc`. This is the simplest fix and matches the behavior of `new[]` / `std::vector` which would throw on OOM.

### Solution B — Return a bool from `grow()` and propagate failure

Make `grow()` return `false` on failure, make `write_data()` check the result, and add a public error state or return value to signal the caller.

### Recommendation

**Solution A** for now. The `Buffer` class is likely dead code (see Issue 19), but if it stays, aborting on OOM is the safest immediate fix. Adding error propagation (Solution B) is more correct long-term but requires changing `write_data`'s signature and all callers.

```cpp
void Lowl::Buffer::grow(const size_t p_length) {
    size_t new_real_length = real_length + p_length;
    void *newloc = realloc(data, new_real_length);
    if (!newloc) {
        std::abort();  // OOM is unrecoverable for this class
    }
    data = static_cast<uint8_t *>(newloc);
    real_length = new_real_length;
}
```

**Fix applied:** Solution A — `std::abort()` on `realloc` failure instead of silent return.

---

## Issue 3 — OGG `SEEK_END` Adds to Position Instead of Setting It — **FIXED**

**Severity:** Critical
**Files:** `src/audio/reader/lowl_audio_reader_ogg.cpp:39-42`

### Problem

```cpp
} else if (origin == SEEK_END) {
    src->index += src->length + (size_t)offset;  // BUG
}
```

`SEEK_END` means "set position to end-of-data plus offset" (offset is typically negative). Using `+=` instead of `=` adds the current position, making the seek go to `current + length + offset` instead of `length + offset`. Any OGG file that triggers a `SEEK_END` (e.g., libvorbis querying stream length) will seek to a wrong position, potentially reading garbage or causing the decoder to fail.

### Solution A — Change `+=` to `=`

Minimal one-character fix: `src->index = src->length + (size_t)offset;`

Note: casting a negative `ogg_int64_t` to `size_t` is technically UB, but this matches how most in-memory OGG callbacks work in practice (offset is always 0 for SEEK_END in libvorbis).

### Solution B — Use signed arithmetic and clamp

Compute the target as a signed value, clamp to `[0, length]`, then assign:

```cpp
} else if (origin == SEEK_END) {
    int64_t target = static_cast<int64_t>(src->length) + offset;
    src->index = static_cast<size_t>(std::clamp<int64_t>(target, 0, src->length));
}
```

### Recommendation

**Solution B.** The signed arithmetic is safer against negative offsets and costs nothing. Apply the same clamping to `SEEK_SET` and `SEEK_CUR` for robustness:

```cpp
static int ogg_memory_seek(void *source, ogg_int64_t offset, int origin) {
    OggData *src = static_cast<OggData *>(source);
    int64_t target = 0;
    if (origin == SEEK_SET) {
        target = offset;
    } else if (origin == SEEK_CUR) {
        target = static_cast<int64_t>(src->index) + offset;
    } else if (origin == SEEK_END) {
        target = static_cast<int64_t>(src->length) + offset;
    }
    src->index = static_cast<size_t>(std::clamp<int64_t>(target, 0, static_cast<int64_t>(src->length)));
    return 0;
}
```

**Fix applied:** Solution B — rewrote all three branches with signed `int64_t` arithmetic and `std::clamp`. Also returns `-1` for invalid `whence`.

---

## Issue 4 — `ReleasePool` Destructor Use-After-Free — **FIXED**

**Severity:** Critical
**Files:** `src/lowl_release_pool.h`

### Problem

Member destruction order is reverse of declaration: `mutex` (destroyed first), then `pool`, then `timer`. The `timer` destructor calls `stop()` which joins the background thread. But that thread's callback (`release_callback`) uses `mutex` and `pool`, which may already be destroyed by the time the thread is joined.

Timeline of destruction:
1. `~ReleasePool()` body runs (empty)
2. `mutex` destroyed
3. `pool` destroyed
4. `timer` destroyed → calls `stop()` → joins thread that may be inside `release_callback()` using the now-destroyed `mutex` and `pool`

### Solution A — Reorder member declarations

Move `timer` to be the first declared member. Since destruction is reverse order, `timer` will be destroyed last, ensuring the thread is joined before `mutex` and `pool` are destroyed.

### Solution B — Explicitly stop the timer in the destructor body

```cpp
~ReleasePool() {
    timer->stop();  // join thread before any member is destroyed
}
```

### Recommendation

**Both changes together.** Solution B is the explicit, self-documenting fix. Solution A provides defense-in-depth if someone later adds another member or changes the destructor. The combined fix is:

```cpp
class ReleasePool {
private:
    std::unique_ptr<Timer> timer;    // declared first, destroyed last
    std::vector<std::shared_ptr<void>> pool;
    std::mutex mutex;
    // ...
public:
    ~ReleasePool() {
        timer->stop();  // explicit: join thread before members are torn down
    }
};
```

The member order already happens to be correct in the current code (`timer` is first). The real fix is adding the explicit `stop()` in the destructor, but verifying and documenting the declaration order provides the safety guarantee.

Actually, re-reading the current code: the declaration order is `timer`, `pool`, `mutex` — so destruction is `mutex` first, then `pool`, then `timer`. To fix via reorder, `timer` should be **last** declared (so it's destroyed first — wait, that's backwards). Let me re-check.

Destruction order is reverse of declaration order. Current: `timer` declared first → destroyed last. That means `mutex` and `pool` are destroyed before `timer`. So the fix is to declare `timer` **last**:

```cpp
class ReleasePool {
private:
    std::mutex mutex;
    std::vector<std::shared_ptr<void>> pool;
    std::unique_ptr<Timer> timer;  // declared last → destroyed first → thread joins before pool/mutex die
```

Or just add `timer->stop()` in the destructor body.

**Recommendation: add `timer->stop()` to the destructor.** It is explicit and does not depend on fragile declaration ordering.

**Fix applied:** Added `timer->stop()` in `~ReleasePool()` destructor body to join the background thread before any member is destroyed.

---

## Issue 5 — `sample_to_uint8` Undefined Behavior on Negative Samples — **FIXED**

**Severity:** Critical
**Files:** `src/audio/convert/lowl_audio_sample_converter.h:69-71`

### Problem

```cpp
uint8_t scaled = (uint8_t)(128 + ((uint8_t)(p_sample * (127.0f))));
```

When `p_sample` is negative (which it will be for roughly half of all audio samples — range is [-1, 1]), `p_sample * 127.0f` produces a negative float. Casting a negative float to `uint8_t` is undefined behavior per the C++ standard. In practice it may wrap or produce 0, but the audio output will be wrong either way.

### Solution A — Cast through a signed intermediate

```cpp
static _INLINE_ uint8_t sample_to_uint8(Lowl::Sample p_sample) {
    int32_t scaled = static_cast<int32_t>(p_sample * 127.0f);
    return static_cast<uint8_t>(128 + scaled);
}
```

This computes the offset in signed arithmetic (well-defined) then converts to unsigned at the final step.

### Solution B — Use the same formula as `uint8_to_float` in reverse

The existing `uint8_to_float` maps `[0, 255]` to `[-1.0, 1.0]` with 128 as the zero-crossing. The inverse:

```cpp
static _INLINE_ uint8_t sample_to_uint8(Lowl::Sample p_sample) {
    float clamped = std::clamp(static_cast<float>(p_sample), -1.0f, 1.0f);
    return static_cast<uint8_t>(static_cast<int>(clamped * 127.0f) + 128);
}
```

### Recommendation

**Solution B.** It adds a clamp for safety (avoids overflow on out-of-range samples) and mirrors the decode path exactly, ensuring round-trip consistency.

**Fix applied:** Solution B — `std::clamp` + signed intermediate. Also replaced `<math.h>` with `<cmath>` and added `<algorithm>` for `std::clamp`.

---

## Issue 6 — `Buffer::read_u16()` / `read_u32()` Unsequenced Side Effects — **FIXED**

**Severity:** High
**Files:** `src/lowl_buffer.cpp:54-62`

### Problem

```cpp
uint16_t value = static_cast<uint16_t>(read_u8() | read_u8() << 8);
```

The C++ standard does not guarantee evaluation order of operands to `|`. Both `read_u8()` calls advance `position`, so if the compiler evaluates right-to-left, the bytes are swapped. `read_u32()` has the same issue with four calls.

### Solution A — Use intermediate variables

```cpp
uint16_t Lowl::Buffer::read_u16() {
    uint8_t b0 = read_u8();
    uint8_t b1 = read_u8();
    return static_cast<uint16_t>(b0 | (b1 << 8));
}
```

### Solution B — Read both bytes at once via `read_data()`

```cpp
uint16_t Lowl::Buffer::read_u16() {
    uint16_t value = 0;
    read_data(&value, sizeof(value));
    // assumes little-endian host, which this code already assumes
    return value;
}
```

### Recommendation

**Solution A.** It is explicit about byte order, makes no endianness assumptions, and clearly sequences the reads. Apply the same pattern to `read_u32()`:

```cpp
uint32_t Lowl::Buffer::read_u32() {
    uint8_t b0 = read_u8();
    uint8_t b1 = read_u8();
    uint8_t b2 = read_u8();
    uint8_t b3 = read_u8();
    return static_cast<uint32_t>(b0 | (b1 << 8) | (b2 << 16) | (b3 << 24));
}
```

**Fix applied:** Solution A — intermediate variables for both `read_u16()` and `read_u32()`.

---

## Issue 7 — `AudioVoice` Constructor Pauses by Default — **FIXED**

**Severity:** High
**Files:** `src/audio/source/lowl_audio_voice.cpp`, `src/audio/source/lowl_audio_space.cpp`

### Problem

`AudioVoice` constructor calls `pause()`, so a newly constructed voice renders silence until `play()` is called. This breaks direct `AudioVoice` usage outside of `AudioSpace` and fails `test_audio_data.cpp`.

The `pause()` was added to support `AudioSpace::create_playback()`, which wants new voices to start stopped. But it leaks an `AudioSpace` concern into the `AudioVoice` constructor.

### Solution A — Remove `pause()` from constructor; make `AudioSpace` call `stop_playback()` after construction

The voice starts in a renderable state by default. `AudioSpace::create_playback()` explicitly calls `stop_playback()` on the newly created voice (which it already does via `insert_playback_locked`).

### Solution B — Add a constructor parameter controlling initial state

```cpp
AudioVoice(std::shared_ptr<const AudioData> p_audio_data, bool p_start_paused = false);
```

`AudioSpace` passes `true`, direct callers get `false` by default.

### Recommendation

**Solution A.** The current `insert_playback_locked` already calls `voice->stop_playback()`, so removing the constructor `pause()` only changes behavior for direct voice construction (which is the case that's broken). No API surface change needed.

In the constructor, replace:
```cpp
pause();
```
with the voice starting in its natural state (`playback_enabled = true` from `AudioSource` base). Set `playback_state` to `Playing` so direct render works. `AudioSpace::insert_playback_locked` already transitions to stopped.

**Fix applied:** Solution A — removed `pause()` from constructor, replaced with `playback_state.store(PlaybackState::Playing)`. `AudioSpace::insert_playback_locked` already calls `stop_playback()`. AudioData test now passes.

---

## Issue 8 — Reset/Seek/Stop Do Not Immediately Update Query State — **FIXED**

**Severity:** High
**Files:** `src/audio/source/lowl_audio_voice.h`, `src/audio/source/lowl_audio_voice.cpp`

### Problem

After calling `stop_playback()`, `seek_frame()`, or `reset()`, the values returned by `get_frame_position()` and `get_frames_remaining()` still reflect the old render position until the next render callback picks up the pending request. This makes the API non-deterministic from the caller's perspective.

### Solution A — Dual-position model: render position + logical position

Introduce a separate `logical_position` atomic that is updated immediately by control methods. Query methods read `logical_position`. The render thread reads and updates the render position, and syncs `logical_position` when consuming a seek request.

### Solution B — Immediately write both `position` and `seek_position` in control methods

On `stop_playback()`, `reset()`, `seek_frame()`, and `seek_time()`: write `position` atomically alongside the seek request. The render thread still processes the reset flag to handle the actual seek, but `get_frame_position()` reads `position` which is already updated.

### Recommendation

**Solution B.** It avoids adding a third atomic field. The key change: control methods immediately write `position.store(target)` in addition to setting `seek_position` and clearing the reset flag. Since the render thread only reads `position` at the start of `render()` and writes it at the end, and control methods are not called from the render thread, the worst case is that the render thread reads the new position and the seek flag — both point to the same target, so no frame is skipped or doubled.

Specifically:
- `reset()`: `position.store(0)`, `seek_position.store(0)`, clear flag
- `seek_frame(f)`: `position.store(f)`, `seek_position.store(f)`, clear flag
- `stop_playback()`: `position.store(0)`, set state to Stopped, pause

**Fix applied:** Solution B — `reset()`, `seek_frame()`, and `stop_playback()` now write `position` atomically alongside `seek_position`. `get_frame_position()` and `get_frames_remaining()` reflect changes immediately.

---

## Issue 9 — Mixer Scratch Buffer Truncates Blocks Larger Than 8192 Frames

**Severity:** High
**Files:** `src/audio/source/lowl_audio_mixer.cpp`

### Problem

`AudioMixer` uses a fixed 8192-frame scratch buffer. If a device requests more than 8192 frames per callback (e.g., a high-latency configuration), the mixer only processes `min(requested, 8192)` frames and silently returns fewer frames than requested. The device's `render_to_device_buffer` zero-fills the remainder, causing periodic silence gaps.

### Solution A — Chunked processing within `render()`

Split the incoming `p_block` into sub-blocks of at most `SCRATCH_BUFFER_CAPACITY` frames. Process each chunk through the existing mix logic. Accumulate total produced frames. No heap allocation on the render path.

### Solution B — Dynamically size the scratch buffer at mixer construction

Accept a `max_frames_per_buffer` parameter and allocate the scratch buffer to match. The device knows its buffer size at `start()` time and can propagate it.

### Recommendation

**Solution A.** It is RT-safe (no allocation), works regardless of the device's buffer size, and doesn't require the mixer to know the device's configuration. The outer loop in `render()` advances a write offset through `p_block` in chunks:

```cpp
uint32_t total_produced = 0;
uint32_t remaining = p_block.frame_count;
while (remaining > 0) {
    uint32_t chunk_size = std::min(remaining, SCRATCH_BUFFER_CAPACITY);
    AudioBlockView chunk = /* sub-view of p_block at offset total_produced, size chunk_size */;
    // ... existing per-source mix logic on chunk ...
    total_produced += chunk_produced;
    remaining -= chunk_size;
}
```

Event processing (Mix/Remove) should happen only once at the top of `render()`, not inside the chunk loop.

---

## Issue 10 — `AudioSpace::render()` Ignores `playback_enabled` Gate

**Severity:** High
**Files:** `src/audio/source/lowl_audio_space.cpp:398-407`

### Problem

`AudioSpace` inherits from `AudioSource` which provides `pause()` / `play()` controlling `playback_enabled`. But `AudioSpace::render()` delegates directly to `mixer->render()` without checking the gate. Calling `space->pause()` has no effect on rendering.

Additionally, `get_frames_remaining()`, `get_frame_position()`, and `get_frame_count()` return placeholder constants (`1`, `0`, `0`) that don't reflect actual state.

### Solution A — Add the gate check; keep placeholder query values

Add an early-out in `render()` when paused. Leave the aggregate query methods returning constants for now, documented as "not applicable for aggregate sources."

### Solution B — Full aggregate query implementation

In addition to the gate, implement meaningful query methods: `get_frames_remaining()` returns the max remaining across active playbacks, etc.

### Recommendation

**Solution A now, Solution B later.** The gate fix is a one-line addition and is clearly correct. The aggregate query semantics require a design decision (max? sum? undefined?) and locking, which should be a separate task.

```cpp
RenderResult Lowl::Audio::AudioSpace::render(AudioBlockView p_block) {
    if (!playback_enabled.load(std::memory_order_relaxed)) {
        return {0, RenderState::Starved};
    }
    // ... existing delegation to mixer ...
}
```

---

## Issue 11 — `Lowl::Lib::drivers` Data Race

**Severity:** High
**Files:** `src/lowl.cpp`

### Problem

`drivers` is a `std::vector<shared_ptr<AudioDriver>>` modified during `initialize()` and read by `get_drivers()` / `get_default_device()` with no synchronization. The `atomic_flag` prevents double-init but does not provide a happens-before relationship for the vector's contents becoming visible to other threads.

### Solution A — Use `std::call_once` for initialization

Replace the `atomic_flag` with `std::once_flag` and `std::call_once`. This guarantees a full memory barrier: all threads calling `get_drivers()` after `call_once` completes will see the fully populated vector.

### Solution B — Protect with a `std::mutex`

Guard all accesses to `drivers` with a mutex. `initialize()` locks and populates, `get_drivers()` locks and copies.

### Recommendation

**Solution A.** `std::call_once` is designed for exactly this pattern (lazy singleton initialization). It's lock-free on the fast path after initialization. `get_drivers()` and `get_default_device()` are safe without a mutex because `drivers` is never modified after `call_once` completes.

```cpp
static std::once_flag init_flag;

void Lowl::Lib::initialize(Error &error) {
    std::call_once(init_flag, [&error]() {
        // ... populate drivers ...
    });
}
```

---

## Issue 12 — `AudioSource::name` Data Race

**Severity:** High
**Files:** `src/audio/source/lowl_audio_source.h`, `src/audio/source/lowl_audio_source.cpp`

### Problem

`set_name()` and `get_name()` access a `std::string` without synchronization. If `set_name()` is called from the main thread while `get_name()` is called from the render thread (e.g., via logging), this is a data race on a heap-allocated object, which is undefined behavior.

### Solution A — Make `name` atomic-friendly by only setting it before the source is active

Document that `set_name()` must be called before the source is added to a mixer. This is a policy fix, not a code fix.

### Solution B — Protect with a mutex or use an atomic shared_ptr to string

Use `std::mutex` around name access, or store name as `std::shared_ptr<const std::string>` with atomic load/store.

### Recommendation

**Solution A with an assertion.** The name is a debugging label, not a hot-path value. It is always set during construction or immediately after. Adding a mutex for a debug string is overkill. Instead, document the contract and optionally add a debug assertion that `set_name()` is only called when the source is not in a mixer (checked via `detached` or a similar flag). For `AudioVoice`, the name is set in `AudioSpace::create_playback()` before the voice is added to the mixer, so current usage is already safe.

---

## Issue 13 — `Logger` Static State Unprotected

**Severity:** Medium
**Files:** `src/lowl_logger.h`, `src/lowl_logger.cpp`

### Problem

`Logger::receiver`, `Logger::user_data`, and `Logger::log_level` are plain static variables. `set_log_level()`, `register_log_receiver()`, and `write()` can be called from different threads, creating data races. Additionally, `pretty_time()` calls `std::localtime()` which uses a static buffer and is not thread-safe.

### Solution A — Use atomics for the static state; replace `localtime` with `localtime_r`

`log_level` → `std::atomic<Level>`. `receiver` → `std::atomic<LogMessageReceiver>`. `user_data` → `std::atomic<void*>`. Replace `std::localtime()` with `localtime_r()` (POSIX) or `localtime_s()` (MSVC).

### Solution B — Accept that logger configuration is set-once at startup

Document that `set_log_level` and `register_log_receiver` must be called before any multi-threaded work. Add a memory fence (or use `std::call_once`) to ensure visibility.

### Recommendation

**Solution A for the atomics, Solution B's mindset for configuration.** Making `log_level` and `receiver` atomic is trivial and has no performance cost (they're read once per log call, not per sample). Replacing `localtime` with the reentrant variant is a platform ifdef but straightforward. The logger is a debug facility — don't over-engineer it, but do make it not-UB.

---

## Issue 14 — Mixer Linear Scan Over 1024 Slots on Real-Time Thread

**Severity:** Medium
**Files:** `src/audio/source/lowl_audio_mixer.cpp`

### Problem

`find_source_index()` and `find_free_source_index()` linearly scan all 1024 `ActiveSourceSlot` entries. This runs on the audio callback thread during event processing. With typical usage (< 32 active sources), most iterations touch cold cache lines of empty slots.

### Solution A — Maintain a count of active sources; stop scanning early

Track `active_count` (incremented on add, decremented on remove). `find_free_source_index` can start scanning from index 0 and stop at `active_count + 1`. `find_source_index` can stop once it has checked `active_count` non-null slots.

### Solution B — Free-list of available slot indices

Maintain a stack/ring of free indices. On add, pop a free index. On remove, push the index back. Eliminates scanning entirely for `find_free_source_index`.

### Recommendation

**Solution A.** It's the minimal change: add one `size_t active_source_count` member, increment/decrement on add/remove, use it to bound the scan in `find_source_index` loops. In practice, with < 32 active sources, the scan terminates almost immediately. A free-list (Solution B) is better in theory but adds complexity to a real-time code path for a case (1024 simultaneous sources) that is unlikely in practice.

---

## Issue 15 — Playback Slot and Asset-ID Monotonic Exhaustion

**Severity:** Medium
**Files:** `src/audio/source/lowl_audio_space.h`, `src/audio/source/lowl_audio_space.cpp`, `src/lowl_typedef.h`

### Problem

Both `AudioPlaybackId` and `AudioAssetId` are `uint16_t` values allocated monotonically. After 65,535 playbacks or assets (across the process lifetime), the IDs wrap to 0 (the invalid sentinel) and the system stops functioning. Playback slots have generation counters but are never reused.

### Solution A — Implement playback slot reuse with a free-list; widen asset IDs

- Playback: maintain a free-list of retired slot indices. When a slot is recycled, push its index. `insert_playback_locked` pops from the free-list before allocating new indices. Generation bumps prevent stale-handle use.
- Assets: widen `AudioAssetId` to `uint32_t`. At 100 assets/second, this lasts 497 days. Good enough for most applications.

### Solution B — Generational handles for both playbacks and assets

Introduce `AudioAssetHandle { id, generation }` matching the existing `AudioPlaybackHandle` pattern. Both use slot reuse with generation counters.

### Recommendation

**Solution A now, Solution B if asset reuse becomes necessary.** Playback slot reuse is the urgent fix because playbacks are created/destroyed frequently (every time a sound plays). Asset IDs are more stable (loaded once, used many times), so widening to `uint32_t` provides adequate headroom without the complexity of a generational handle.

Implementation for playback reuse:
- Add `std::vector<AudioPlaybackId> free_playback_slots` to `AudioSpace`.
- In `recycle_playback_locked()`: push the slot ID onto `free_playback_slots`.
- In `insert_playback_locked()`: pop from `free_playback_slots` before falling through to monotonic allocation.
- Keep the generation bump on recycle (already implemented).

---

## Issue 16 — CoreAudio `get_num_channel` Memory Leak and Wrong Allocation

**Severity:** Medium
**Files:** `src/audio/backend/coreaudio/lowl_audio_core_audio_utilities.cpp:82-98`

### Problem

```cpp
AudioBufferList *audio_buffers = new AudioBufferList[stream_config_data_size];
```

This allocates `stream_config_data_size` copies of `AudioBufferList` (each ~24 bytes), when the intent is to allocate `stream_config_data_size` bytes. It works by massively over-allocating. On the error path (line 88), the function returns without calling `delete[]`, leaking the allocation.

### Solution A — Use a `std::vector<uint8_t>` as backing storage

```cpp
std::vector<uint8_t> buffer(stream_config_data_size);
AudioBufferList *audio_buffers = reinterpret_cast<AudioBufferList *>(buffer.data());
```

This is RAII, correctly sized, and automatically cleaned up on all paths.

### Solution B — Use `std::unique_ptr<uint8_t[]>`

```cpp
auto buffer = std::make_unique<uint8_t[]>(stream_config_data_size);
AudioBufferList *audio_buffers = reinterpret_cast<AudioBufferList *>(buffer.get());
```

### Recommendation

**Solution A.** `std::vector` is idiomatic, exception-safe, and the buffer is stack-local so there's no ownership ambiguity. Replace `delete[] audio_buffers` at line 96 with nothing (RAII handles it).

---

## Issue 17 — ELF-Only Linker Flags Applied on macOS Clang Builds

**Severity:** Medium
**Files:** `CMakeLists.txt:173-178`

### Problem

```cmake
target_link_options(${LOWL_LIB_MAIN} PRIVATE
    LINKER:-z,relro
    LINKER:-z,now
    LINKER:-z,noexecstack
    LINKER:-z,separate-code
)
```

These `-z` flags are Linux/ELF-only. On macOS, the Apple linker (`ld64`/`ld_prime`) does not understand them. Depending on CMake version and toolchain, this either produces warnings or errors.

### Solution A — Guard with `if(UNIX AND NOT APPLE)`

```cmake
if (UNIX AND NOT APPLE)
    target_link_options(...)
endif()
```

### Solution B — Guard with a linker feature check

Use `check_linker_flag(CXX "-z,relro" HAS_Z_RELRO)` and conditionally apply.

### Recommendation

**Solution A.** Simple platform guard. The flags are security hardening for Linux deployments. macOS has equivalent protections enabled by default (`-Wl,-bind_at_load` etc.) that don't need to be explicitly set.

Additionally, `set(LOWL_DRIVER_WASAPI TRUE)` on line 19 is set unconditionally but only used on Windows. While harmless (the sources are `#ifdef`-guarded), it would be cleaner to set it inside `if(WIN32)`.

---

## Issue 18 — `format_log()` Argument Order Inconsistency

**Severity:** Low
**Files:** `src/lowl_logger.cpp:107-127`

### Problem

The first `snprintf` (size measurement) passes arguments in order: `function_name`, `message`, `file_name`, `line`. The second `snprintf` (formatting) passes: `message`, `function_name`, `file_name`, `line`. The total character count is identical by coincidence (sum of string lengths doesn't depend on order), so no buffer overflow occurs. But the size-measurement call formats a different string than the output call.

### Solution A — Fix the argument order in the first call to match the second

Make both calls pass `message, function_name, file_name, line`.

### Solution B — Use a single `std::string` formatting approach

Replace the two-pass `snprintf` with `Lowl::Logger::format_arguments()` (which already exists and handles dynamic sizing).

### Recommendation

**Solution A.** It's a one-line swap. The two-pass snprintf pattern is fine; just make the arguments consistent.

---

## Issue 19 — `Buffer` Class: Dead Code, Rule-of-Five Violation, Raw Pointer from `slice()`

**Severity:** Low
**Files:** `src/lowl_buffer.h`, `src/lowl_buffer.cpp`

### Problem

Multiple issues in the `Buffer` class:
1. **Appears unused** — no audio pipeline code references `Lowl::Buffer`. The audio path uses `AudioBuffer`, `unique_ptr<Sample[]>`, `unique_ptr<uint8_t[]>`, and `std::vector`.
2. **Rule of Five violation** — has a destructor (`free(data)`) but no copy/move constructors or assignment operators. Copying would double-free.
3. **`slice()` returns raw `new Buffer(...)`** — caller must manually `delete`, easy to leak.
4. **BSWAP macros defined but never used** — dead code.

### Solution A — Delete the class entirely

If no code uses it, remove `lowl_buffer.h`, `lowl_buffer.cpp`, and the CMakeLists entry. This eliminates Issues 2, 3, 6 (partially) at once.

### Solution B — Fix the class: add Rule of Five, return `unique_ptr` from `slice()`, remove unused macros

Make `Buffer` non-copyable, add move constructor/assignment, change `slice()` to return `unique_ptr<Buffer>`.

### Recommendation

**Solution A — delete it.** Verify first with a grep that no code in `src/` or `test/` references `Lowl::Buffer`. If it's truly unused, removing dead code is better than maintaining it. If some external code (the Godot wrapper) uses it, apply Solution B.

---

## Issue 20 — `SampleConverter` C-Style Casts, Misnamed Variable, `_INLINE_` Misuse

**Severity:** Low
**Files:** `src/audio/convert/lowl_audio_sample_converter.h`, `src/audio/lowl_audio_sample_format.h`

### Problem

1. **C-style casts throughout** — `(int16_t)(...)`, `(uint8_t)(...)`, etc. Inconsistent with the rest of the codebase.
2. **`sample_to_int8` names its variable `int16`** (line 75) — obvious copy-paste error.
3. **`_INLINE_` (`always_inline`) on `sample_format_to_string`** — this function returns `std::string` (heap allocation). Force-inlining it just bloats code size.

### Solution A — Fix individually

Replace C-style casts with `static_cast`, rename `int16` to `int8_val`, remove `_INLINE_` from `sample_format_to_string`.

### Solution B — Broader cleanup of `_INLINE_` usage

Audit all `_INLINE_` uses. Reserve it for functions that are (a) trivial, (b) called in the render hot path, and (c) don't allocate. Remove it from `sample_format_to_string`, `audio_channel_mask_string`, `get_channel_num`, `get_channel`, and `get_sample_size_*`.

### Recommendation

**Solution A for the immediate fixes, Solution B as a follow-up.** The cast and variable name fixes are obvious correctness improvements. The `_INLINE_` audit is a separate cleanup pass.

---

## Issue 21 — `AudioDeviceProperties` Uninitialized Members; `get_closest_properties()` Stub

**Severity:** Low
**Files:** `src/audio/backend/lowl_audio_device_properties.h`, `src/audio/backend/lowl_audio_device.cpp`

### Problem

1. `AudioDeviceProperties` has no default member initializers. A default-constructed instance has indeterminate values for `is_supported`, `sample_rate`, `channel`, `sample_format`, `channel_map`, `exclusive_mode`. Most call sites use `AudioDeviceProperties{}` (value-init → zero), but omitting the braces produces garbage.
2. `get_closest_properties()` is a stub: empty loop body, always returns `properties_list[0]`.

### Solution A — Add default member initializers

```cpp
struct AudioDeviceProperties {
    bool is_supported = false;
    SampleRate sample_rate = NO_SAMPLE_RATE;
    AudioChannel channel = AudioChannel::None;
    SampleFormat sample_format = SampleFormat::Unknown;
    AudioChannelMask channel_map = AudioChannelMask::NONE;
    bool exclusive_mode = false;
    // ...
};
```

### Solution B — Make the struct aggregate-initializable only (delete default constructor)

Force all construction to use designated initializers or brace-init with explicit values.

### Recommendation

**Solution A.** Default member initializers are zero-cost and prevent accidental use of garbage values. The `get_closest_properties()` stub should remain a tracked TODO — it's a feature gap, not a bug, since the CoreAudio backend currently enumerates all supported properties and callers select one explicitly.

---

## Issue 22 — `Timer` Uses Raw `new std::thread`

**Severity:** Low
**Files:** `src/lowl_timer.h`

### Problem

`Timer` allocates threads with `new std::thread(...)` and frees with `delete thread`. If `start_interval` or `start_timer` is called while a thread is already running, `stop()` is called first (which joins and deletes), then a new thread is allocated. This works but is fragile: any exception between `new` and the eventual `delete` would leak.

### Solution A — Use `std::unique_ptr<std::thread>`

Replace `std::thread *thread` with `std::unique_ptr<std::thread>`. `stop()` calls `thread->join()` then `thread.reset()`. Automatic cleanup on all paths.

### Solution B — Use `std::jthread` (C++20)

`std::jthread` auto-joins on destruction and supports cooperative cancellation via `stop_token`, eliminating the manual `running` flag.

### Recommendation

**Solution A.** The project targets C++17, so `std::jthread` is not available. `unique_ptr<std::thread>` is the minimal RAII fix.

---

## Issue 23 — Driver Test Conflates Platform Availability with Core Regressions

**Severity:** Low
**Files:** `test/test_driver.cpp`

### Problem

`test_driver.cpp` hard-fails when a platform backend (CoreAudio/WASAPI) cannot initialize on the local machine. This makes the full test suite noisy and hides whether failures come from the core library or the local audio environment (e.g., CI machines with no audio device).

### Solution A — Split into core and environment tests

Separate the test into two cases:
- "Driver Enumeration" (always runs): tests that `Lowl::Lib::initialize()` succeeds and returns a non-empty driver list (at least the dummy driver).
- "Device Initialization" (environment-dependent): marked with a tag or skip condition when no real audio device is present.

### Solution B — Accept known "backend unavailable" errors as non-fatal

Instead of `REQUIRE(error.ok())`, use `CHECK` or `WARN` for backend-specific initialization, and only `REQUIRE` for the core library path.

### Recommendation

**Solution A.** Clean separation makes it obvious what broke. The dummy driver should always be available and testable regardless of platform. Device-specific tests can be gated on a runtime check (e.g., `driver->get_devices().empty()`).

---

## Suggested Execution Sequence

| Phase | Issues | Rationale |
|-------|--------|-----------|
| **1 — Fix silent data corruption** | #1, #5, #3, #6 | These produce wrong audio output or read wrong data. Users cannot detect them without bit-level inspection. |
| **2 — Fix crashes and UB** | #2, #4 | Buffer overflow on OOM and use-after-free on shutdown. |
| **3 — Fix behavioral bugs** | #7, #8, #10 | Restore correct Voice/Space semantics; unblock failing tests. |
| **4 — Fix thread safety** | #11, #12, #13 | Eliminate data races that are technically UB even if they don't crash today. |
| **5 — Fix truncation** | #9 | Mixer correctly handles all block sizes. |
| **6 — Lifecycle and resources** | #15, #16 | Playback slot reuse; CoreAudio memory leak. |
| **7 — Build and quality** | #17, #18, #19, #20, #21, #22 | Linker flags, dead code removal, code quality. |
| **8 — Test hygiene** | #23 | Clean test separation. |

## Success Criteria

- All INT_24, uint8, and OGG output is bit-correct.
- No undefined behavior in any sample conversion or buffer operation.
- `AudioVoice` direct usage works; `test_audio_data` passes.
- Seek/reset/stop immediately reflect in query methods.
- `AudioSpace::pause()` actually pauses rendering.
- No data races in `Lib`, `Logger`, or `AudioSource::name`.
- Mixer processes arbitrarily large blocks without truncation.
- Playback slots are reused; stale handles remain invalid.
- CoreAudio builds have no ELF linker flag warnings.
- Full test suite clearly separates core regressions from platform availability.
