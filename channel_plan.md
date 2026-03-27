# Channel Layout Migration Plan

## Goal

Replace the current count-based `AudioChannel` + device-only `AudioChannelMask` model with a single `ChannelLayout` type that survives from file decode to mixer render to device output, as described in `channel.md`.

## Review of Previous Plan and Key Changes

The previous plan had the right overall sequencing (type first, pipeline, backends, cleanup) and correct technical decisions (WASAPI-aligned bits, CoreAudio UseChannelBitmap, AudioBlockView unchanged). This revision addresses the following gaps:

1. **Step 2 was too large** -- changed 12 files at once. Now split into 3 sub-steps.
2. **Step 5 (strict validation) came before step 6 (converter)** -- tightening layout matching before the converter can handle layout conversion breaks `AudioSpace::add_audio`. Reordered.
3. **Bit position shift not called out** -- the old `AudioChannelMask` has `MONO` at bit 0, `LEFT` at bit 1. The new `Speaker` has `FrontLeft` at bit 0 (WASAPI order). The CoreAudio layout mapping tables need value updates, not just type changes.
4. **`constexpr` named layouts need a constexpr popcount** -- C++17 doesn't have `std::popcount`. Need a constexpr helper.
5. **`AudioSpace::add_audio` conversion gap** -- during migration there is a window where the converter can't handle layout mismatches. Plan now addresses this.
6. **No mention of Godot wrapper impact** -- noted as external dependency.
7. **Missing error/edge case behavior for ChannelLayout API** -- now specified.
8. **issuev2.md interactions** -- several existing bugs should be fixed alongside this migration.

## Current Constraints

- `AudioBuffer` and `AudioBlockView` already work as raw planar storage indexed by integer channel. No structural rewrite needed.
- The highest-risk breakpoints are `AudioSource` / `AudioData`, `AudioDeviceProperties`, `ChannelConverter`, file readers, and the CoreAudio / WASAPI backends.
- CoreAudio already has mapping code for tags, bitmaps, and descriptions in `src/audio/backend/coreaudio/lowl_audio_core_audio_layout.cpp`. The mapping table values need updating (old bit positions -> new WASAPI-aligned positions), but the structure is reusable.
- WASAPI conversion collapses to a cast once `Speaker` bits match `dwChannelMask`.
- The Godot wrapper (out of scope for this plan) will break when constructor signatures change. That is expected and will be fixed separately.

## Recommended Order

### Step 1 -- Introduce `Speaker` and `ChannelLayout`

**Files:**
- `src/audio/lowl_audio_channel.h` (add new types alongside old ones)
- new: `test/test_channel_layout.cpp`

**Why first:** Everything else depends on the new type existing. Adding it alongside the old types means zero breakage.

**Implement:**

```cpp
enum class Speaker : uint32_t {
    FrontLeft           = 1 << 0,   // WASAPI bit 0
    FrontRight          = 1 << 1,   // WASAPI bit 1
    FrontCenter         = 1 << 2,   // WASAPI bit 2
    LowFrequency        = 1 << 3,   // WASAPI bit 3
    BackLeft            = 1 << 4,   // WASAPI bit 4
    BackRight           = 1 << 5,   // WASAPI bit 5
    FrontLeftOfCenter   = 1 << 6,   // WASAPI bit 6
    FrontRightOfCenter  = 1 << 7,   // WASAPI bit 7
    BackCenter          = 1 << 8,   // WASAPI bit 8
    SideLeft            = 1 << 9,   // WASAPI bit 9
    SideRight           = 1 << 10,  // WASAPI bit 10
    TopCenter           = 1 << 11,  // WASAPI bit 11
    TopFrontLeft        = 1 << 12,  // WASAPI bit 12
    TopFrontCenter      = 1 << 13,  // WASAPI bit 13
    TopFrontRight       = 1 << 14,  // WASAPI bit 14
    TopBackLeft         = 1 << 15,  // WASAPI bit 15
    TopBackCenter       = 1 << 16,  // WASAPI bit 16
    TopBackRight        = 1 << 17,  // WASAPI bit 17
};
```

```cpp
struct ChannelLayout {
    uint32_t speaker_mask = 0;
    uint8_t channel_count = 0;

    // Construction
    static constexpr ChannelLayout from_mask(uint32_t mask);
    static constexpr ChannelLayout from_count(uint8_t count);

    // Query
    constexpr Speaker speaker_at(uint8_t channel_index) const;
    constexpr int index_of(Speaker speaker) const;  // -1 if absent
    constexpr bool has(Speaker speaker) const;
    constexpr bool is_valid() const { return channel_count > 0; }

    // Comparison
    constexpr bool operator==(const ChannelLayout &other) const;
    constexpr bool operator!=(const ChannelLayout &other) const;

    // Debug
    std::string to_string() const;

    // Named layouts
    static constexpr ChannelLayout Mono;
    static constexpr ChannelLayout Stereo;
    static constexpr ChannelLayout Surround_3_0;
    static constexpr ChannelLayout Quad;
    static constexpr ChannelLayout Quad_Side;
    static constexpr ChannelLayout Surround_4_0;
    static constexpr ChannelLayout Surround_5_0;
    static constexpr ChannelLayout Surround_5_0_Rear;
    static constexpr ChannelLayout Surround_5_1;
    static constexpr ChannelLayout Surround_5_1_Rear;
    static constexpr ChannelLayout Surround_6_1;
    static constexpr ChannelLayout Surround_7_1;
    static constexpr ChannelLayout Surround_7_1_Front;
};
```

**`constexpr` popcount:** C++17 doesn't have `std::popcount`. Add a constexpr helper:

```cpp
static constexpr uint8_t popcount32(uint32_t x) {
    x = x - ((x >> 1) & 0x55555555);
    x = (x & 0x33333333) + ((x >> 2) & 0x33333333);
    return static_cast<uint8_t>(((x + (x >> 4)) & 0x0F0F0F0F) * 0x01010101 >> 24);
}
```

**Edge cases to define:**
- `from_mask(0)` returns `{0, 0}` (invalid/empty layout)
- `from_count(0)` returns `{0, 0}`
- `from_count(n)` for n > 8 returns `{0, 0}` (unsupported)
- `speaker_at(i)` for `i >= channel_count` returns `static_cast<Speaker>(0)` (no speaker)
- `index_of(speaker)` returns -1 if speaker not in layout

**`from_count()` defaults:**

```
1 -> Mono         {FrontCenter}
2 -> Stereo       {FrontLeft, FrontRight}
3 -> Surround_3_0 {FrontLeft, FrontRight, FrontCenter}
4 -> Quad         {FrontLeft, FrontRight, BackLeft, BackRight}
5 -> Surround_5_0 {FrontLeft, FrontRight, FrontCenter, SideLeft, SideRight}
6 -> Surround_5_1 {FrontLeft, FrontRight, FrontCenter, LowFrequency, SideLeft, SideRight}
7 -> Surround_6_1 {FrontLeft, FrontRight, FrontCenter, LowFrequency, SideLeft, SideRight, BackCenter}
8 -> Surround_7_1 {FrontLeft, FrontRight, FrontCenter, LowFrequency, BackLeft, BackRight, SideLeft, SideRight}
```

Note: count 4 defaults to Quad (rear) which matches WAVE convention. The old code used `Quadraphonic` which is ambiguous. If this is a breaking change for existing callers that expected side surrounds, they should use `ChannelLayout::Quad_Side` explicitly.

**Checkpoint:**
- `ChannelLayout::from_count(1..8)` produces correct layouts.
- `speaker_at()` ordering matches ascending bit position.
- Named layouts are constexpr-constructed and equal to their from_mask equivalents.
- Tests cover stereo, 5.1 side, 5.1 rear, 7.1 with explicit speaker verification.
- Old `AudioChannel` / `AudioChannelMask` still exist and compile.

---

### Step 2a -- Move `AudioSource` and `AudioData` to `ChannelLayout`

**Files:**
- `src/audio/source/lowl_audio_source.h`
- `src/audio/source/lowl_audio_source.cpp`
- `src/audio/source/lowl_audio_data.h`
- `src/audio/source/lowl_audio_data.cpp`

**Why this sub-step:** These are the two foundational types. Everything else inherits or wraps them.

**Implement:**
- Replace `AudioChannel channel` with `const ChannelLayout channel_layout` in `AudioSource` (fixes issuev2 #47 -- makes it const).
- Replace `AudioChannel channel` with `ChannelLayout channel_layout` in `AudioData`.
- Add `get_channel_layout()` and `get_channel_count()`.
- Keep a temporary bridge: `AudioChannel get_channel() const` that returns an approximate AudioChannel from channel_count. Remove this in step 11.

```cpp
class AudioSource {
protected:
    const SampleRate sample_rate;       // issuev2 #47: now const
    const ChannelLayout channel_layout; // replaces AudioChannel channel

public:
    AudioSource(SampleRate p_sample_rate, ChannelLayout p_channel_layout);
    ChannelLayout get_channel_layout() const { return channel_layout; }
    uint8_t get_channel_count() const { return channel_layout.channel_count; }
    // Temporary bridge -- remove in step 11:
    AudioChannel get_channel() const;
};
```

**Checkpoint:**
- Everything compiles (callers still use `get_channel()` bridge).
- Existing tests pass unchanged.

---

### Step 2b -- Move `AudioVoice`, `AudioStream`, `AudioMixer`, `AudioSpace` to `ChannelLayout`

**Files:**
- `src/audio/source/lowl_audio_voice.h` / `.cpp`
- `src/audio/source/lowl_audio_stream.h` / `.cpp`
- `src/audio/source/lowl_audio_mixer.h` / `.cpp`
- `src/audio/source/lowl_audio_space.h` / `.cpp`

**Why this sub-step:** All inherit from `AudioSource`. Their constructors need to pass `ChannelLayout` to the base.

**Implement:**
- Change constructor signatures from `AudioChannel` to `ChannelLayout`.
- `AudioVoice` gets its layout from `AudioData::get_channel_layout()` -- no API change needed.
- `AudioStream`, `AudioMixer`, `AudioSpace` constructors take `ChannelLayout` instead of `AudioChannel`.
- Keep `AudioSpace::add_audio` using the old `ChannelConverter` for now (it calls `get_channel()` bridge). This will be updated in step 6.

**Checkpoint:**
- All constructors accept `ChannelLayout`.
- Existing tests updated: `AudioSpace(48000.0, AudioChannel::Stereo)` -> `AudioSpace(48000.0, ChannelLayout::Stereo)`.
- Mono/stereo paths still work end-to-end.

---

### Step 2c -- Update utilities and `AudioSource::get_properties()`

**Files:**
- `src/audio/lowl_audio_utilities.h` / `.cpp`
- `src/audio/source/lowl_audio_source.cpp` (`get_properties()`)

**Implement:**
- `ms_to_samples()` takes `uint8_t channel_count` instead of `AudioChannel`.
- `AudioSource::get_properties()` populates `channel_layout` instead of (or alongside) `channel`.

**Checkpoint:**
- Utility tests pass.
- `get_properties()` returns layout info.

---

### Step 3 -- Make panning speaker-aware

**Files:**
- `src/audio/source/lowl_audio_source.cpp` (`process_panning`)

**Why here:** Small, self-contained fix that removes the "channel 0 = left, channel 1 = right" assumption.

**Implement:**

```cpp
void AudioSource::process_panning(AudioBlockView p_block) {
    const Volume pan = panning.load(std::memory_order_relaxed);
    if (pan == DEFAULT_PANNING) return;
    const Volume clamped = std::clamp(pan, static_cast<Volume>(-1), static_cast<Volume>(1));  // issuev2 #21
    const int left_idx = channel_layout.index_of(Speaker::FrontLeft);
    const int right_idx = channel_layout.index_of(Speaker::FrontRight);
    if (left_idx >= 0) {
        const Volume left_gain = static_cast<Volume>(std::sqrt(static_cast<Sample>(1) - clamped));
        Sample *left = p_block.channel(static_cast<uint8_t>(left_idx));
        for (uint32_t i = 0; i < p_block.frame_count; i++) left[i] *= left_gain;
    }
    if (right_idx >= 0) {
        const Volume right_gain = static_cast<Volume>(std::sqrt(static_cast<Sample>(1) + clamped));
        Sample *right = p_block.channel(static_cast<uint8_t>(right_idx));
        for (uint32_t i = 0; i < p_block.frame_count; i++) right[i] *= right_gain;
    }
}
```

Note: The clamp on `pan` fixes issuev2 #21 (sqrt of negative -> NaN).

**Checkpoint:**
- Stereo panning tests still pass.
- A 5.1 render only changes FL/FR. Center/LFE/surround channels are unaffected.

---

### Step 4 -- Collapse `AudioDeviceProperties` to `ChannelLayout`

**Files:**
- `src/audio/backend/lowl_audio_device_properties.h`
- `src/audio/backend/lowl_audio_device.cpp`
- `src/audio/backend/lowl_audio_device.h`
- `demo/main.cpp`
- `test/test_audio_device.cpp`

**Why before backends:** Source/device matching should use the same layout type before platform work starts.

**Implement:**
- Replace `AudioChannel channel` + `AudioChannelMask channel_map` with `ChannelLayout channel_layout`.
- Update `to_string()`, equality, ordering, and closest-match scoring.
- Fix `operator<` consistency (issuev2 #77): both `==` and `<` should use exact layout comparison.

```cpp
struct AudioDeviceProperties {
    bool is_supported = false;
    SampleRate sample_rate = NO_SAMPLE_RATE;
    ChannelLayout channel_layout;               // replaces channel + channel_map
    SampleFormat sample_format = SampleFormat::Unknown;
    bool exclusive_mode = false;
    AudioDevicePropertiesWasapi wasapi{};
};
```

Scoring change:
```cpp
const bool layout_mismatch =
    p_requested.channel_layout.is_valid() &&
    p_candidate.channel_layout != p_requested.channel_layout;
```

**Checkpoint:**
- `get_closest_properties()` prefers exact layout match.
- 5.1 side vs 5.1 rear are distinguishable.

---

### Step 5 -- Rebuild `ChannelConverter` around layout mapping tables

**Files:**
- `src/audio/convert/lowl_audio_channel_converter.h`
- `src/audio/convert/lowl_audio_channel_converter.cpp`
- new or extended: `test/test_channel_converter.cpp`

**Why before strict validation (not after, as in the old plan):** `AudioSpace::add_audio` calls the converter when a loaded asset's layout doesn't match the space's layout. If we tighten validation (step 6) before the converter supports layout conversion, loading any non-matching asset breaks. The converter must be ready first.

**Implement in two passes:**

**Pass 1 -- direct and remap conversions:**
- Same-layout: no-op (return shared_ptr or copy).
- Mono <-> Stereo: existing logic, adapted to ChannelLayout.
- Same-count remap (5.1 side <-> 5.1 rear): channel identity swap.

**Pass 2 -- count-changing upmix/downmix:**
- Stereo -> 5.1: copy FL/FR, zero FC/LFE/SL/SR.
- 5.1 -> Stereo: ITU-R BS.775 fold-down:
  `L_out = FL + 0.707*FC + 0.707*SL`
  `R_out = FR + 0.707*FC + 0.707*SR`
- 7.1 -> 5.1: fold BL/BR into SL/SR or drop.

**API change:**

```cpp
class ChannelConverter {
public:
    std::unique_ptr<AudioData> convert(
        ChannelLayout p_target_layout,
        std::shared_ptr<AudioData> p_source,
        Error &error) const;
};
```

**Implementation pattern:** Build a mix plan mapping each destination speaker to source speaker(s) with gains:

```cpp
struct ChannelContribution {
    int src_index;    // -1 for silence
    float gain;
};
// For each target channel, a list of contributions from source channels.
```

Unsupported conversions: set `ErrorCode::ConvertAudioChannelNotSupported` and return nullptr.

**Checkpoint:**
- Existing mono/stereo tests pass with new API.
- New tests: side/rear remap, 5.1 -> stereo downmix, stereo -> 5.1 upmix.

---

### Step 6 -- Update mixer and device validation to require layout equality

**Files:**
- `src/audio/source/lowl_audio_mixer.cpp`
- `src/audio/source/lowl_audio_space.cpp` (`add_audio`)
- `src/audio/backend/lowl_audio_device.cpp`

**Why after converter:** Now that the converter handles layout mismatches, we can safely tighten validation.

**Implement:**
- Mixer: replace `get_channel() != channel` with `get_channel_layout() != channel_layout`.
- `AudioSpace::add_audio`: convert using `ChannelLayout` instead of `AudioChannel`. The converter from step 5 now handles the new API.
- Device `start()`: validate layout match, not just count match.

```cpp
if (p_audio_source->get_channel_layout() != channel_layout) {
    LOWL_LOG_ERROR("layout mismatch");
    return;
}
```

**Checkpoint:**
- Mixer rejects `Surround_5_1` vs `Surround_5_1_Rear`.
- `AudioSpace::add_audio` auto-converts a stereo asset into a 5.1 space.
- Device render still writes interleaved samples in canonical order.

---

### Step 7 -- Change reader APIs to construct `AudioData` with `ChannelLayout`

**Files (can be done in parallel per reader):**
- `src/audio/reader/lowl_audio_reader.h` / `.cpp`
- `src/audio/reader/lowl_audio_reader_wav.cpp`
- `src/audio/reader/lowl_audio_reader_flac.cpp`
- `src/audio/reader/lowl_audio_reader_ogg.cpp`
- `src/audio/reader/lowl_audio_reader_opus.cpp`
- `src/audio/reader/lowl_audio_reader_mp3.cpp`
- `test/test_audio_reader.cpp`

**Implement:**

Change `create_audio_data` helper:
```cpp
std::unique_ptr<AudioData> create_audio_data(
    AudioFormat p_audio_format,
    SampleFormat p_sample_format,
    ChannelLayout p_layout,       // was: AudioChannel
    SampleRate p_sample_rate,
    const std::unique_ptr<uint8_t[]> &p_buffer,
    size_t p_size,
    Error &error);
```

**Per-reader layout extraction:**

| Reader | Layout source | Fallback |
|--------|--------------|----------|
| WAV | `dwChannelMask` from `WAVEFORMATEXTENSIBLE` | `from_count(wav.channels)` |
| FLAC | Standard Vorbis channel order by count | `from_count(channels)` |
| Ogg Vorbis | Vorbis spec channel mapping per count | `from_count(channels)` |
| Opus | `OpusHead` channel mapping family | `from_count(channels)` |
| MP3 | Always mono or stereo | `from_count(channels)` |

**Checkpoint:**
- Reader tests verify planar conversion still works.
- WAV files with `dwChannelMask` produce correct layouts.
- Coverage for 3, 5, 7 channel layouts via `from_count`.

---

### Step 8 -- Simplify WASAPI backend around direct mask reuse

**Files:**
- `src/audio/backend/wasapi/lowl_audio_wasapi_device.h`
- `src/audio/backend/wasapi/lowl_audio_wasapi_device.cpp`

**Why before CoreAudio:** WASAPI is simpler since the internal bits now match `dwChannelMask`.

**Implement:**
- Replace `to_channel_mask()` / `to_wasapi_channel_mask()` with direct `speaker_mask` use.
- In `to_audio_device_properties()`: `ChannelLayout::from_mask(wfe->dwChannelMask)` for extensible, `from_count(nChannels)` for plain WAVEFORMATEX.
- In `to_wave_format_extensible()`: `wfe.dwChannelMask = channel_layout.speaker_mask`.

```cpp
properties.channel_layout =
    wave_format_extensible->dwChannelMask != 0
        ? ChannelLayout::from_mask(wave_format_extensible->dwChannelMask)
        : ChannelLayout::from_count(p_wave_format_ex->nChannels);

wfe.Format.nChannels = channel_layout.channel_count;
wfe.dwChannelMask = channel_layout.speaker_mask;
```

**Checkpoint:**
- 5.1 side vs rear survives round-trip through `WAVEFORMATEXTENSIBLE`.
- `start()` rejects layout mismatches.

---

### Step 9 -- Convert CoreAudio backend to `ChannelLayout`, prefer `UseChannelBitmap`

**Files:**
- `src/audio/backend/coreaudio/lowl_audio_core_audio_layout.h`
- `src/audio/backend/coreaudio/lowl_audio_core_audio_layout.cpp`
- `src/audio/backend/coreaudio/lowl_audio_core_audio_utilities.h` / `.cpp`
- `src/audio/backend/coreaudio/lowl_audio_core_audio_device.cpp`

**Critical note -- bit position shift:**

The old `AudioChannelMask` has different bit positions than the new `Speaker` enum:

| Speaker | Old `AudioChannelMask` bit | New `Speaker` bit |
|---------|---------------------------|-------------------|
| Mono | 0 | (removed -- use FrontCenter) |
| Left/FrontLeft | 1 | 0 |
| Right/FrontRight | 2 | 1 |
| FrontCenter | 3 | 2 |
| LowFrequency | 4 | 3 |
| BackLeft | 5 | 4 |
| BackRight | 6 | 5 |
| ... | ... | ... |

This means every mask constant in `lowl_audio_core_audio_layout.cpp` (the `kStereoMask`, `kFiveOneSideMask`, etc.) must be rewritten with new values. It is not just a type rename.

**Implement:**
- Change `to_channel_mask()` -> `to_channel_layout()` returning `ChannelLayout`.
- Update all mask constants to use `Speaker` enum values.
- `to_channel_mask_from_label()` -> `to_speaker()` returning `Speaker`.
- `to_channel_mask_from_bitmap()` -> `to_channel_layout_from_bitmap()`.
- `create_channel_layout_data()` uses `kAudioChannelLayoutTag_UseChannelBitmap`:

```cpp
std::vector<uint8_t> create_channel_layout_data(const ChannelLayout &layout) {
    std::vector<uint8_t> data(sizeof(AudioChannelLayout), 0);
    auto *raw = reinterpret_cast<AudioChannelLayout *>(data.data());
    raw->mChannelLayoutTag = kAudioChannelLayoutTag_UseChannelBitmap;
    raw->mChannelBitmap = static_cast<AudioChannelBitmap>(layout.speaker_mask);
    return data;
}
```

This is a significant simplification: we no longer need `to_channel_layout_tag()` for output. We keep the tag-based lookup table only for reading device capabilities.

**Checkpoint:**
- CoreAudio discovery preserves 3/5/7 channel layouts.
- Output configuration uses UseChannelBitmap.
- No more ambiguous tag-only assumptions.

---

### Step 10 -- Expand device probing to test multiple layouts

**Files:**
- `src/audio/backend/coreaudio/lowl_audio_core_audio_device.cpp`
- `src/audio/backend/wasapi/lowl_audio_wasapi_device.cpp`

**Why late:** Probing only becomes meaningful after both backends understand `ChannelLayout`.

**Implement:**
- Keep the native/default device layout.
- Probe a small fixed set of common alternatives:

```cpp
static const ChannelLayout kProbeLayouts[] = {
    ChannelLayout::Mono,
    ChannelLayout::Stereo,
    ChannelLayout::Surround_5_1,
    ChannelLayout::Surround_7_1,
};
```

For each layout x sample rate x format:
- WASAPI: `IsFormatSupported()` with `dwChannelMask = layout.speaker_mask`.
- CoreAudio: set stream format + channel layout, check success.
- Skip duplicates via layout equality.

**Performance note:** This is a one-time cost at device initialization. With 4 layouts x ~5 sample rates x ~3 formats = ~60 calls per device. Typical WASAPI `IsFormatSupported` takes <1ms. Total <100ms per device.

**Checkpoint:**
- A stereo-default device can report validated 5.1/7.1 alternatives when the platform accepts them.

---

### Step 11 -- Remove legacy channel API

**Files:**
- Every remaining `AudioChannel`, `AudioChannelMask`, `get_channel_num()`, `get_channel()` usage across `src/`, `test/`, `demo/`, docs.

**Why last:** Cleanup, not prerequisite. Removing too early turns a controlled migration into a flag day.

**Remove:**
- `AudioChannel` enum
- `AudioChannelMask` enum and its operator overloads
- `get_channel_num(AudioChannel)` free function
- `get_channel(uint32_t)` free function
- `audio_channel_mask_string()` function
- The `AudioSource::get_channel()` bridge added in step 2a

**Verify removal is complete:**
```bash
rg -n "AudioChannel[^L]|AudioChannelMask|get_channel_num\(|get_channel\(" src test demo
```
Should return zero hits (excluding `ChannelLayout` references).

**Checkpoint:**
- Full test suite passes.
- No references to old types remain.

---

## Test Plan

### Step 1 tests (new file: `test/test_channel_layout.cpp`)

- `from_count(0)` -> invalid layout
- `from_count(1..8)` -> correct named layouts
- `from_count(9)` -> invalid layout
- `from_mask(0)` -> invalid layout
- `from_mask(FL|FR)` -> Stereo
- `speaker_at()` returns speakers in ascending bit order
- `index_of()` round-trips with `speaker_at()`
- `index_of(absent_speaker)` -> -1
- `has()` checks
- Named layout equality: `ChannelLayout::Surround_5_1 == from_count(6)`
- `Surround_5_1 != Surround_5_1_Rear`
- `to_string()` output

### Step 3 tests (panning)

- Stereo: panning left attenuates FR, boosts FL.
- 5.1: panning only affects FL/FR; FC/LFE/SL/SR unchanged.
- Mono (FrontCenter only): panning has no effect (no FL/FR).

### Step 5 tests (converter)

- Same layout -> pass-through (no copy, or identical copy).
- Mono -> Stereo: both channels equal to mono source.
- Stereo -> Mono: average of L and R.
- 5.1 side -> 5.1 rear: SL->BL, SR->BR, rest unchanged.
- 5.1 -> Stereo: ITU-R downmix values.
- Stereo -> 5.1: FL/FR copied, rest zero.
- Unsupported conversion -> error.

### Pipeline regression tests

- Raw interleaved data -> planar AudioData -> AudioVoice -> render.
- 5.1 clip through AudioData -> AudioStream -> render: speaker ordering stable.
- `AudioSpace::add_audio` with mismatched layout triggers auto-conversion.

### Backend tests (where possible on the platform)

- WASAPI: `speaker_mask` round-trips through `WAVEFORMATEXTENSIBLE`.
- CoreAudio: tag/bitmap/description -> `ChannelLayout` covers all mapped tags.
- CoreAudio: `ChannelLayout` -> `UseChannelBitmap` -> correct `mChannelBitmap`.

---

## issuev2.md Interactions

These existing bugs should be fixed as part of this migration:

| issuev2 # | Fix in step | Description |
|-----------|-------------|-------------|
| #21 | Step 3 | `process_panning` sqrt of negative -> add clamp |
| #47 | Step 2a | `sample_rate` and `channel` should be const -> make `channel_layout` const |
| #77 | Step 4 | `operator<` inconsistent with fuzzy `operator==` -> use exact layout comparison |

---

## Practical Notes

- `MAX_CHANNELS` stays at 8 for this pass. Sufficient for all proposed layouts. Can raise to 18 for top-channel support later without design change.
- `AudioBlockView` and `AudioBuffer` are unchanged. Count-based planar buffers remain correct once count comes from `ChannelLayout`.
- Land as a sequence of compile-green commits:
  1. New type + tests
  2. AudioSource + AudioData
  3. Voice + Stream + Mixer + Space
  4. Utilities + get_properties
  5. Panning fix
  6. Device properties
  7. Converter rewrite
  8. Strict validation
  9. Readers (can be parallelized)
  10. WASAPI backend
  11. CoreAudio backend
  12. Device probing
  13. Legacy removal
- Each commit should compile and pass the full test suite.

## Definition of Done

- Layout identity is preserved from decode / stream creation to mixer to device configuration.
- 3, 5, and 7 channel layouts are representable and no longer collapse to "None".
- Source/device and source/mixer matching reject layout mismatches, not just count mismatches.
- Panning uses speaker identity (FL/FR), not channel index assumptions.
- ChannelConverter handles mono/stereo/5.1/7.1 conversions and side<->rear remaps.
- WASAPI uses `speaker_mask` directly as `dwChannelMask`.
- CoreAudio output uses `UseChannelBitmap` for canonical ordering.
- Readers extract layout from file headers where available.
- Legacy `AudioChannel` / `AudioChannelMask` types are fully removed.
- All tests pass.
