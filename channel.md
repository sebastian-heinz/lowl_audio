# Channel Layout Analysis

## Current State

### Two separate types model channels

**`AudioChannel`** (enum, `lowl_audio_channel.h:8`) — a channel *count* disguised as a layout:

```
None=0, Mono=1, Stereo=2, Quadraphonic=4, Surround5_1=6, Surround7_1=8
```

The numeric value IS the count. It is the primary type used across the entire pipeline: `AudioSource`, `AudioData`, `AudioVoice`, `AudioStream`, `AudioMixer`, `AudioSpace`, `ChannelConverter`, and all readers.

**`AudioChannelMask`** (bitmask enum, `lowl_audio_channel.h:17`) — 19 individual speaker position flags:

```
MONO, LEFT, RIGHT, FRONT_CENTER, LOW_FREQUENCY, BACK_LEFT, BACK_RIGHT,
FRONT_LEFT_OF_CENTER, FRONT_RIGHT_OF_CENTER, BACK_CENTER, SIDE_LEFT, SIDE_RIGHT,
TOP_CENTER, TOP_FRONT_LEFT, TOP_FRONT_CENTER, TOP_FRONT_RIGHT,
TOP_BACK_LEFT, TOP_BACK_CENTER, TOP_BACK_RIGHT
```

This mask is stored in `AudioDeviceProperties::channel_map` and both backends convert to/from their platform representations. But it **never enters the audio pipeline** — no `AudioSource`, `AudioData`, or `AudioBlockView` carries it.

### Where each type is used

| Component | Uses `AudioChannel` | Uses `AudioChannelMask` |
|-----------|:-------------------:|:-----------------------:|
| `AudioSource` | channel count | -- |
| `AudioData` | channel count | -- |
| `AudioVoice` | inherits from AudioSource | -- |
| `AudioStream` | inherits from AudioSource | -- |
| `AudioMixer` | inherits from AudioSource | -- |
| `AudioSpace` | inherits from AudioSource | -- |
| `AudioBlockView` | channel_count (uint8_t) | -- |
| `AudioBuffer` | channel_count (uint8_t) | -- |
| `AudioDeviceProperties` | channel (AudioChannel) | channel_map (AudioChannelMask) |
| `ChannelConverter` | convert target | -- |
| CoreAudio backend | device properties | layout discovery & configuration |
| WASAPI backend | device properties | dwChannelMask conversion |
| File readers | decoded AudioData | -- |

---

## Gaps

### GAP 1: `AudioChannel` conflates count with layout

A count of 6 could mean:
- 5.1 with **side** surrounds (FL, FR, FC, LFE, SL, SR) — MPEG 5.1
- 5.1 with **rear** surrounds (FL, FR, FC, LFE, BL, BR) — WAVE 5.1

A count of 4 could mean:
- Quadraphonic with sides (FL, FR, SL, SR)
- Quadraphonic with rears (FL, FR, BL, BR)

A count of 8 could mean:
- 7.1 surround (FL, FR, FC, LFE, BL, BR, SL, SR)
- 7.1 front (FL, FR, FC, LFE, FLC, FRC, SL, SR)

The `AudioChannel` enum can only represent one layout per count. The pipeline has no way to distinguish between layouts with the same channel count.

### GAP 2: Counts 3, 5, 7 are unrepresentable

`get_channel(uint32_t)` returns `AudioChannel::None` for channel counts 3, 5, and 7. Real devices and files can have:

| Count | Example Layout |
|-------|---------------|
| 3 | 3.0 (FL, FR, FC) |
| 5 | 5.0 (FL, FR, FC, SL, SR) |
| 7 | 6.1 (FL, FR, FC, LFE, BC, SL, SR) or 7.0 (FL, FR, FC, BL, BR, SL, SR) |

A CoreAudio device reporting 3 output channels would get `AudioChannel::None` and be treated as having no valid channel configuration.

### GAP 3: `AudioChannelMask` never flows into the audio pipeline

The mask is discovered at the device level but immediately forgotten. When a device is started:

```cpp
// CoreAudioDevice::start() at line 165:
if (p_audio_source->get_channel() != p_audio_device_properties.channel)
    // error — but this only compares AudioChannel (count), not layout
```

The `AudioSource` has no `channel_map` field. There is no way for any source, mixer, or voice to carry or report its actual speaker layout.

### GAP 4: Channel index-to-speaker mapping is implicit

`render_to_device_buffer()` (`lowl_audio_device.cpp:99-104`):

```cpp
for (uint32_t frame_index = 0; frame_index < produced_frames; frame_index++) {
    for (uint8_t channel_index = 0; channel_index < output_block.channel_count; channel_index++) {
        const Sample sample = output_block.channel(channel_index)[frame_index];
        SampleConverter::write_sample(..., sample, &write_ptr);
    }
}
```

Channel index 0 goes to the first interleaved position, index 1 to the second, etc. There is no remapping. The code assumes the internal channel ordering matches the device's expected interleave order, but this mapping is never defined or enforced.

WASAPI expects channels interleaved in `dwChannelMask` bit order. CoreAudio expects channels in the order defined by its `AudioChannelLayoutTag`. Without an explicit mapping, multichannel audio will have speakers swapped (e.g., LFE data playing from the center speaker).

### GAP 5: Device discovery only probes stereo for non-default modes

Both `CoreAudioDevice::create_device_properties()` (line 382) and `WasapiDevice::create_device_properties()` (line 848) hardcode:

```cpp
test_properties.channel = AudioChannel::Stereo;
test_properties.channel_map = AudioChannelMask::LEFT | AudioChannelMask::RIGHT;
```

A device that supports 5.1 or 7.1 will only report those layouts if they happen to be the device's default configuration. All the sample-rate/format combinations are tested only at stereo.

### GAP 6: `ChannelConverter` only handles Mono <-> Stereo

`lowl_audio_channel_converter.cpp:41-62`:

```cpp
if (p_from == AudioChannel::Mono && p_to == AudioChannel::Stereo) { ... }
else if (p_from == AudioChannel::Stereo && p_to == AudioChannel::Mono) { ... }
else {
    error.set_error(ErrorCode::ConvertAudioChannelNotSupported);
}
```

No upmixing (stereo to 5.1), no downmixing (5.1 to stereo), no layout remapping (5.1-side to 5.1-rear).

### GAP 7: Panning only affects first two channels

`AudioSource::process_panning()` (`lowl_audio_source.cpp:72-93`) applies gain to `channel(0)` and `channel(1)`. For a 5.1 or 7.1 source, the center, LFE, and surround channels are unaffected by panning.

### GAP 8: `AudioBlockView` carries no layout identity

```cpp
struct AudioBlockView {
    std::array<Sample*, MAX_CHANNELS> channels;
    uint32_t frame_count;
    uint8_t channel_count;
    // No layout information
};
```

When the mixer adds samples from source A into the output block, it does `dst[channel_index] += src[channel_index]`. If source A is stereo (2 channels) and the mixer is 5.1 (6 channels), the stereo source writes to indices 0 and 1, which happen to map to FL and FR. This works by accident for the common case but there is no check and no explicit routing.

### GAP 9: File readers discard channel layout metadata

WAV files contain a `dwChannelMask` field in the `WAVEFORMATEXTENSIBLE` header that specifies exactly which speakers each channel maps to. FLAC, Opus, and Vorbis also have channel layout definitions. The readers extract the channel count but not the layout.

### GAP 10: `AudioSource::get_properties()` omits `channel_map`

```cpp
AudioDeviceProperties AudioSource::get_properties() const {
    AudioDeviceProperties properties{};
    properties.channel = get_channel();
    properties.sample_format = get_sample_format();
    properties.sample_rate = get_sample_rate();
    return properties;  // channel_map defaults to NONE
}
```

Any code matching a source to a device via properties will never match on channel layout.

### GAP 11: `MONO` mask bit position creates ambiguity

`MONO = (1 << 0)` occupies bit 0 while `LEFT = (1 << 1)` occupies bit 1. WASAPI does not have a MONO speaker flag — mono is represented as `SPEAKER_FRONT_CENTER`. CoreAudio has `kAudioChannelLabel_Mono` as a separate concept from `kAudioChannelLabel_Center`. The MONO bit is an internal concept that doesn't map cleanly to either platform.

---

## Platform Channel Layouts

### WASAPI Speaker Positions (`dwChannelMask` bits)

| Bit | WASAPI Constant | Speaker |
|-----|----------------|---------|
| 0 | `SPEAKER_FRONT_LEFT` | Front Left |
| 1 | `SPEAKER_FRONT_RIGHT` | Front Right |
| 2 | `SPEAKER_FRONT_CENTER` | Front Center |
| 3 | `SPEAKER_LOW_FREQUENCY` | LFE / Subwoofer |
| 4 | `SPEAKER_BACK_LEFT` | Back Left |
| 5 | `SPEAKER_BACK_RIGHT` | Back Right |
| 6 | `SPEAKER_FRONT_LEFT_OF_CENTER` | Front Left of Center |
| 7 | `SPEAKER_FRONT_RIGHT_OF_CENTER` | Front Right of Center |
| 8 | `SPEAKER_BACK_CENTER` | Back Center |
| 9 | `SPEAKER_SIDE_LEFT` | Side Left |
| 10 | `SPEAKER_SIDE_RIGHT` | Side Right |
| 11 | `SPEAKER_TOP_CENTER` | Top Center |
| 12 | `SPEAKER_TOP_FRONT_LEFT` | Top Front Left |
| 13 | `SPEAKER_TOP_FRONT_CENTER` | Top Front Center |
| 14 | `SPEAKER_TOP_FRONT_RIGHT` | Top Front Right |
| 15 | `SPEAKER_TOP_BACK_LEFT` | Top Back Left |
| 16 | `SPEAKER_TOP_BACK_CENTER` | Top Back Center |
| 17 | `SPEAKER_TOP_BACK_RIGHT` | Top Back Right |

**18 speaker positions.** Channels are always interleaved in ascending bit order.

### CoreAudio Layout Tags (common configurations)

| Tag | Channels | Speakers (in CoreAudio order) |
|-----|----------|-------------------------------|
| `Mono` | 1 | C |
| `Stereo` | 2 | L, R |
| `StereoHeadphones` | 2 | L, R |
| `MPEG_3_0_A` | 3 | L, R, C |
| `MPEG_3_0_B` | 3 | C, L, R |
| `Quadraphonic` | 4 | L, R, SL, SR |
| `WAVE_4_0_B` | 4 | L, R, BL, BR |
| `MPEG_4_0_A` | 4 | L, R, C, BC |
| `MPEG_5_0_A` | 5 | L, R, C, SL, SR |
| `MPEG_5_0_B` | 5 | L, R, SL, SR, C |
| `WAVE_5_0_B` | 5 | L, R, C, BL, BR |
| `MPEG_5_1_A` | 6 | L, R, C, LFE, SL, SR |
| `MPEG_5_1_B` | 6 | L, R, SL, SR, C, LFE |
| `WAVE_5_1_B` | 6 | L, R, C, LFE, BL, BR |
| `MPEG_6_1_A` | 7 | L, R, C, LFE, SL, SR, BC |
| `MPEG_7_1_A` | 8 | L, R, C, LFE, SL, SR, FLC, FRC |
| `MPEG_7_1_B` | 8 | L, R, C, LFE, SL, SR, BL, BR |
| `MPEG_7_1_C` / `WAVE_7_1` | 8 | L, R, C, LFE, BL, BR, SL, SR |

CoreAudio also supports `UseChannelBitmap` (like WASAPI) and `UseChannelDescriptions` (per-channel labels with optional coordinates).

### Union of all speaker positions

Both platforms support the same 18 physical speaker positions. The library should use these as the universal set.

---

## Proposed Design

### Core principle

Replace the dual `AudioChannel` (count) + `AudioChannelMask` (bitmask) system with a single `ChannelLayout` type that captures **which speakers are present and in what order**.

### `Speaker` — individual speaker position flags

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

Bit positions match WASAPI's `SPEAKER_*` constants exactly. This makes WASAPI conversion a cast. The old `MONO` bit is removed — mono is represented as `{FrontCenter}`.

### `ChannelLayout` — the single source of truth

```cpp
struct ChannelLayout {
    uint32_t speaker_mask = 0;   // OR'd Speaker flags — defines which speakers
    uint8_t  channel_count = 0;  // popcount(speaker_mask), cached

    // Query
    Speaker  speaker_at(uint8_t channel_index) const;  // channel 0 -> which speaker?
    int      index_of(Speaker speaker) const;           // FrontLeft -> which channel? (-1 if absent)
    bool     has(Speaker speaker) const;                // is this speaker in the layout?

    // Comparison
    bool     operator==(const ChannelLayout& other) const;

    // Construction
    static ChannelLayout from_mask(uint32_t mask);
    static ChannelLayout from_count(uint8_t count);  // best-guess default layout for count

    // Named layouts (constexpr)
    static constexpr ChannelLayout Mono;            // FC
    static constexpr ChannelLayout Stereo;          // FL, FR
    static constexpr ChannelLayout Surround_3_0;    // FL, FR, FC
    static constexpr ChannelLayout Quad;            // FL, FR, BL, BR
    static constexpr ChannelLayout Quad_Side;       // FL, FR, SL, SR
    static constexpr ChannelLayout Surround_4_0;    // FL, FR, FC, BC
    static constexpr ChannelLayout Surround_5_0;    // FL, FR, FC, SL, SR
    static constexpr ChannelLayout Surround_5_0_Rear;  // FL, FR, FC, BL, BR
    static constexpr ChannelLayout Surround_5_1;    // FL, FR, FC, LFE, SL, SR
    static constexpr ChannelLayout Surround_5_1_Rear;  // FL, FR, FC, LFE, BL, BR
    static constexpr ChannelLayout Surround_6_1;    // FL, FR, FC, LFE, SL, SR, BC
    static constexpr ChannelLayout Surround_7_1;    // FL, FR, FC, LFE, BL, BR, SL, SR
    static constexpr ChannelLayout Surround_7_1_Front; // FL, FR, FC, LFE, FLC, FRC, SL, SR
};
```

**Channel ordering rule**: channels are always ordered by ascending `Speaker` bit position. This is the canonical order. It matches WASAPI's interleave convention. For a 5.1 layout (`FL|FR|FC|LFE|SL|SR`):

```
channel 0 = FrontLeft      (bit 0)
channel 1 = FrontRight     (bit 1)
channel 2 = FrontCenter    (bit 2)
channel 3 = LowFrequency   (bit 3)
channel 4 = SideLeft       (bit 9)
channel 5 = SideRight      (bit 10)
```

`speaker_at()` walks the set bits in order and returns the Nth one. `index_of()` counts how many bits below the target are set.

### `from_count()` — backwards compatibility and convenience

For cases where only a channel count is known (some audio files, simple API usage):

```
1 -> Mono        {FC}
2 -> Stereo      {FL, FR}
3 -> Surround_3_0 {FL, FR, FC}
4 -> Quad        {FL, FR, BL, BR}
5 -> Surround_5_0 {FL, FR, FC, SL, SR}
6 -> Surround_5_1 {FL, FR, FC, LFE, SL, SR}
7 -> Surround_6_1 {FL, FR, FC, LFE, SL, SR, BC}
8 -> Surround_7_1 {FL, FR, FC, LFE, BL, BR, SL, SR}
```

These defaults match the WAV/WASAPI convention. If a reader can extract more specific layout info from the file header, it should use `from_mask()` instead.

---

## Changes Per Component

### `AudioBlockView` and `AudioBuffer`

No change needed. They already carry `channel_count` and index channels by integer. The `ChannelLayout` lives on the source/device that owns the block — the view is just a slice of memory.

`MAX_CHANNELS` stays at 8 unless top-channel support is needed (raise to 18 for full speaker set).

### `AudioSource`

```cpp
class AudioSource {
protected:
    SampleRate sample_rate;
    ChannelLayout channel_layout;  // replaces: AudioChannel channel;
    ...
public:
    AudioSource(SampleRate p_sample_rate, ChannelLayout p_channel_layout);
    ChannelLayout get_channel_layout() const;
    uint8_t get_channel_count() const;  // delegates to channel_layout.channel_count
    ...
};
```

`process_panning()` remains stereo-aware — it operates on FL and FR if present, looked up via `channel_layout.index_of(Speaker::FrontLeft)` and `channel_layout.index_of(Speaker::FrontRight)`. Surround channels are unaffected (correct behavior — panning is a stereo concept; spatial audio needs a different API).

### `AudioData`

```cpp
class AudioData {
    ChannelLayout channel_layout;  // replaces: AudioChannel channel;
    ...
public:
    AudioData(std::unique_ptr<Sample[]> p_storage, size_t p_frame_count,
              SampleRate p_sample_rate, ChannelLayout p_channel_layout);
    ChannelLayout get_channel_layout() const;
    uint8_t get_channel_count() const;
};
```

### `AudioVoice`, `AudioStream`, `AudioMixer`, `AudioSpace`

All inherit from `AudioSource` and get `ChannelLayout` automatically. Constructor signatures change from `AudioChannel` to `ChannelLayout`.

The mixer's `mix()` method already checks that source and mixer channel configurations match. This check becomes:

```cpp
if (p_audio_source->get_channel_layout() != channel_layout) {
    // reject — layouts must match
}
```

This is stricter and more correct than the current count-only check.

### `AudioDeviceProperties`

```cpp
struct AudioDeviceProperties {
    bool is_supported = false;
    SampleRate sample_rate = NO_SAMPLE_RATE;
    ChannelLayout channel_layout;  // replaces: AudioChannel channel + AudioChannelMask channel_map
    SampleFormat sample_format = SampleFormat::Unknown;
    bool exclusive_mode = false;
    AudioDevicePropertiesWasapi wasapi{};
};
```

The separate `AudioChannel channel` and `AudioChannelMask channel_map` fields collapse into one `ChannelLayout`. All the scoring/matching logic in `get_closest_properties()` compares layouts directly.

### `AudioDevice::render_to_device_buffer()`

No change to the interleave loop. Since internal channel ordering matches WASAPI bit order (which is canonical), and the `ChannelLayout` on the device properties tells the backend what order to expect, the existing loop is correct:

```cpp
for (uint32_t frame = 0; frame < produced_frames; frame++) {
    for (uint8_t ch = 0; ch < output_block.channel_count; ch++) {
        Sample sample = output_block.channel(ch)[frame];
        SampleConverter::write_sample(format, sample, &write_ptr);
    }
}
```

Channel `ch` corresponds to `device_properties.channel_layout.speaker_at(ch)`, and the interleaved output matches the layout the backend was told to use.

### CoreAudio Backend

**Device discovery** (`CoreAudioDevice::create_device_properties()`):

```cpp
// Current: converts CoreAudio layout -> AudioChannelMask -> AudioChannel (loses info)
// New: converts CoreAudio layout -> ChannelLayout (preserves everything)

ChannelLayout device_layout = CoreAudioLayout::to_channel_layout(device_id, scope);
AudioDeviceProperties default_properties;
default_properties.channel_layout = device_layout;
```

**Layout configuration** (`CoreAudioLayout`):

```cpp
// to_channel_layout(): CoreAudio AudioChannelLayout -> ChannelLayout
ChannelLayout to_channel_layout(const AudioChannelLayout& layout);

// to_layout_tag(): ChannelLayout -> CoreAudio AudioChannelLayoutTag
AudioChannelLayoutTag to_layout_tag(const ChannelLayout& layout);

// create_channel_layout_data(): ChannelLayout -> serialized AudioChannelLayout for AudioUnit
std::vector<uint8_t> create_channel_layout_data(const ChannelLayout& layout);
```

The existing mapping code in `lowl_audio_core_audio_layout.cpp` already handles the tag/bitmap/description conversions. The functions just change their input/output types.

**CoreAudio channel ordering**: CoreAudio layout tags define specific channel orderings that may differ from canonical. For example, `MPEG_5_1_B` orders channels as L, R, SL, SR, C, LFE (not the canonical L, R, C, LFE, SL, SR).

However, when the library sets `kAudioUnitProperty_AudioChannelLayout` on the AudioUnit input scope, it tells CoreAudio "my data is in THIS layout". CoreAudio's internal conversion handles remapping to the output device's physical speaker arrangement. So the library sends data in canonical order and tells CoreAudio that's the layout — CoreAudio does the rest.

Specifically: use `kAudioChannelLayoutTag_UseChannelBitmap` with the layout's `speaker_mask` as the bitmap. This explicitly tells CoreAudio the data is in WASAPI/WAV bit order. CoreAudio natively understands this mode (it is how WAV files specify layout).

### WASAPI Backend

**Device discovery** (`WasapiDevice::create_device_properties()`):

```cpp
// Current:
properties.channel = get_channel(wfx->nChannels);
properties.channel_map = to_channel_mask(wfe->dwChannelMask);

// New:
properties.channel_layout = ChannelLayout::from_mask(wfe->dwChannelMask);
// If not WAVEFORMATEXTENSIBLE:
properties.channel_layout = ChannelLayout::from_count(wfx->nChannels);
```

**Format configuration** (`to_wave_format_extensible()`):

```cpp
wfe.Format.nChannels = layout.channel_count;
wfe.dwChannelMask = layout.speaker_mask;  // direct — bit positions match
```

No conversion function needed. The `Speaker` enum bits are WASAPI's `SPEAKER_*` bits.

### `ChannelConverter`

Becomes layout-aware. Two categories of conversion:

**1. Layout-preserving count changes** (upmix/downmix):

```
Mono -> Stereo:         FC -> FL=FC, FR=FC (duplicate)
Stereo -> Mono:         FL,FR -> FC=(FL+FR)*0.5
Stereo -> 5.1:          FL,FR -> FL, FR, FC=0, LFE=0, SL=0, SR=0
5.1 -> Stereo:          FL, FR, FC, LFE, SL, SR -> FL=FL+0.707*FC+0.707*SL, FR=FR+0.707*FC+0.707*SR
7.1 -> 5.1:             drop BL/BR, map SL/SR -> SL/SR (or fold BL/BR into SL/SR)
```

**2. Layout remapping** (same count, different speakers):

```
5.1_Side -> 5.1_Rear:   SL->BL, SR->BR (channel identity swap)
Quad_Side -> Quad_Rear:  SL->BL, SR->BR
```

Implementation approach:

```cpp
class ChannelConverter {
public:
    // Converts audio data from one layout to another
    std::unique_ptr<AudioData> convert(
        ChannelLayout p_target_layout,
        std::shared_ptr<AudioData> p_source,
        Error& error) const;
};
```

The converter builds a mapping table: for each target channel, which source channel(s) contribute and at what gain. Channels present in both layouts copy directly. Missing channels get silence or a standard downmix formula (ITU-R BS.775).

### File Readers

Readers should extract channel layout when the format provides it:

- **WAV**: `WAVEFORMATEXTENSIBLE.dwChannelMask` -> `ChannelLayout::from_mask(mask)`
- **FLAC**: FLAC defines standard layouts for 1-8 channels (Vorbis channel order)
- **Ogg Vorbis**: Vorbis spec defines channel mappings per count
- **Opus**: OpusHead defines channel mapping family and specific layouts
- **MP3**: Always mono or stereo

Readers that can only determine count use `ChannelLayout::from_count(n)`.

```cpp
// In reader, when creating AudioData:
AudioData(std::move(storage), frame_count, sample_rate, channel_layout);
// Where channel_layout comes from the file header or from_count(n)
```

### Device Discovery — Probing Multiple Layouts

Currently both backends only test stereo for non-default configurations. With the new design, device discovery should probe the device's native layout plus common alternatives:

```cpp
// Layouts to test (in addition to the device's default):
static const ChannelLayout test_layouts[] = {
    ChannelLayout::Mono,
    ChannelLayout::Stereo,
    ChannelLayout::Surround_5_1,
    ChannelLayout::Surround_7_1,
};
```

For each test layout, combined with each test sample rate and format, call the platform's format validation:
- WASAPI: `IAudioClient::IsFormatSupported()` with the layout's mask in `dwChannelMask`
- CoreAudio: `AudioUnitSetProperty(kAudioUnitProperty_StreamFormat)` with matching channel count, then `set_audio_unit_channel_layout()` with the layout tag

---

## End-to-End Flow

### 1. Device Discovery

```
Platform API
    |
    v
Backend driver enumerates devices
    |
    v
For each device:
    - Query native channel layout -> ChannelLayout
    - Probe additional layouts (stereo, 5.1, 7.1, ...)
    - Build AudioDeviceProperties list with ChannelLayout
    |
    v
User gets: device.get_properties_list()
    -> [{48000Hz, Stereo, Float32}, {48000Hz, 5.1, Float32}, ...]
```

### 2. Audio Loading

```
File on disk
    |
    v
Reader decodes audio
    - Extracts channel layout from file header (WAV dwChannelMask, etc.)
    - Falls back to ChannelLayout::from_count(n) if format has no layout
    |
    v
AudioData with ChannelLayout
    - channel_layout.speaker_at(0) == Speaker::FrontLeft
    - channel_layout.speaker_at(1) == Speaker::FrontRight
    - etc.
```

### 3. Playback Setup

```
User selects device properties (or uses default)
    |
    v
User creates AudioSpace/AudioMixer with matching ChannelLayout
    - AudioSpace space(48000.0, ChannelLayout::Surround_5_1);
    |
    v
User adds audio assets
    - If asset layout != space layout:
        Option A: ChannelConverter converts AudioData to target layout
        Option B: Error — user must convert explicitly
    |
    v
User starts device with source
    - device.start(properties, audio_space, error);
    - Device verifies: source.get_channel_layout() == properties.channel_layout
```

### 4. Render Path (Audio Thread)

```
Device audio callback fires (CoreAudio/WASAPI thread)
    |
    v
render_to_device_buffer()
    |
    v
AudioBuffer::view() -> AudioBlockView (channel_count channels, planar)
    |
    v
audio_source->render(block_view)
    |
    v
AudioMixer/AudioSpace renders all voices into block
    - Each voice copies its channel data by index
    - All sources have the same ChannelLayout, so index N always means the same speaker
    |
    v
Interleave planar -> interleaved in canonical (bit) order
    |
    v
Write to device buffer
    - WASAPI: data is already in dwChannelMask bit order (canonical = WASAPI order)
    - CoreAudio: AudioUnit is configured with UseChannelBitmap matching canonical order
    |
    v
Platform routes to physical speakers
```

### 5. Streaming (AudioStream)

```
User creates AudioStream with ChannelLayout
    - AudioStream stream(48000.0, ChannelLayout::Stereo);
    |
    v
User writes samples via write_interleaved() or write_planar()
    - Interleaved data is de-interleaved using channel_count from layout
    - Channel index 0 = layout.speaker_at(0), etc.
    |
    v
Stream renders into AudioBlockView on audio thread
    - Same flow as above
```

---

## Migration Summary

| File | Change |
|------|--------|
| `lowl_audio_channel.h` | Replace `AudioChannel` + `AudioChannelMask` with `Speaker` + `ChannelLayout` |
| `lowl_audio_device_properties.h` | Replace `channel` + `channel_map` with `channel_layout` |
| `lowl_audio_source.h/cpp` | `AudioChannel channel` -> `ChannelLayout channel_layout` |
| `lowl_audio_data.h/cpp` | `AudioChannel channel` -> `ChannelLayout channel_layout` |
| `lowl_audio_voice.h/cpp` | Constructor takes `ChannelLayout` via `AudioData` |
| `lowl_audio_stream.h/cpp` | Constructor takes `ChannelLayout` |
| `lowl_audio_mixer.h/cpp` | Constructor takes `ChannelLayout`; channel match check uses layout |
| `lowl_audio_space.h/cpp` | Constructor takes `ChannelLayout` |
| `lowl_audio_buffer.h/cpp` | No change (already uses raw count) |
| `lowl_audio_device.h/cpp` | `get_channel_num()` calls -> `channel_layout.channel_count` |
| `lowl_audio_core_audio_device.cpp` | Use `ChannelLayout` in properties; configure AudioUnit with bitmap layout |
| `lowl_audio_core_audio_layout.h/cpp` | Convert between CoreAudio tags and `ChannelLayout`; use `UseChannelBitmap` for output |
| `lowl_audio_wasapi_device.cpp` | `dwChannelMask` <-> `speaker_mask` (trivial cast) |
| `lowl_audio_channel_converter.h/cpp` | Accept `ChannelLayout` target; implement surround downmix/upmix |
| All readers | Extract layout from file headers; construct `AudioData` with `ChannelLayout` |
| `lowl_audio_utilities.h/cpp` | Update `ms_to_samples()` to take count instead of `AudioChannel` |

### Removed types
- `AudioChannel` — replaced by `ChannelLayout`
- `AudioChannelMask` — replaced by `Speaker` flags and `ChannelLayout::speaker_mask`
- `get_channel_num()` free function — replaced by `ChannelLayout::channel_count`
- `get_channel(uint32_t)` free function — replaced by `ChannelLayout::from_count()`

### `MAX_CHANNELS`

Currently 8. This covers up to 7.1. To support height channels (Atmos-style layouts with Top speakers), raise to at least 12 (7.1.4) or 18 (all speaker positions). For now, 8 is sufficient for the standard surround layouts both platforms commonly use. Can be raised later without changing the design.
