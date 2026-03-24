# Issue 6: AudioFrame Hard-Coded Stereo / False Multi-Channel Contract

## Problem

`AudioFrame` stores only `left`/`right`. `operator[]` returns `right` for any index > 0. But `AudioChannel` exposes `Quadraphonic` (4ch) and `AudioChannelMask` defines up to 18 channels. If a backend probes a quad device, `AudioSpace` will "succeed" while silently discarding channels 2-3.

**Files:** `lowl_audio_frame.h:7-38`, `lowl_audio_channel.h`, `lowl_audio_source.cpp:59-84`

---

## Solution A: Scope Down to Mono/Stereo

Remove the false multi-channel contract. Keep `AudioFrame` as-is with `left`/`right`. Delete `Quadraphonic` from `AudioChannel`, strip the unused upper masks from `AudioChannelMask`, and add a `static_assert` or runtime check that rejects >2 channel configs at the API boundary.

### Changes

- `lowl_audio_channel.h` -- remove `Quadraphonic = 4`, remove masks beyond `RIGHT`, `get_channel()` returns `None` for anything > 2
- `lowl_audio_frame.h` -- add `static_assert(MAX_CHANNEL == 2)` comment clarifying mono/stereo scope
- `lowl_audio_source.cpp` -- remove the `Quadraphonic` case from `process_panning`
- Backend device negotiation -- reject or downmix configs with >2 channels

### Pros

- Zero runtime cost, zero API surface change for existing users
- Matches what the library actually implements today -- no silent bugs
- Smallest diff, fastest to ship, unblocks work on issues #1-5
- `AudioChannelMask` can be re-expanded later when N-channel is actually built

### Cons

- Closes the door on multi-channel until explicitly reopened
- If Godot integration needs quad output soon, this delays it

### Best for

A library that is currently mono/stereo-only in practice, with higher-priority correctness bugs (#1-3) and an eventual DSP pipeline planned. Honest API > aspirational API.

---

## Solution B: Fixed-Capacity Channel Array

Replace `left`/`right` with `std::array<Sample, MAX_CHANNEL>` where `MAX_CHANNEL` is a compile-time constant (e.g., 8 for 7.1). Keep named accessors `left()`/`right()` as inline helpers.

```cpp
struct AudioFrame {
    static constexpr unsigned int MAX_CHANNEL = 8;
    std::array<Sample, MAX_CHANNEL> channels{};
    uint8_t active_channels = 2;

    _INLINE_ Sample &left()  { return channels[0]; }
    _INLINE_ Sample &right() { return channels[1]; }
    _INLINE_ Sample &operator[](int idx) { return channels[idx]; }
    // arithmetic ops iterate over active_channels
};
```

### Changes

- `lowl_audio_frame.h` -- full rewrite of the struct
- Every file touching `audio_frame.left` / `audio_frame.right` (~28 files, 106 occurrences) -- migrate to `left()`/`right()` or `[0]`/`[1]`
- `lowl_audio_mixer.cpp` -- normalization loop iterates `active_channels`
- `lowl_audio_source.cpp` -- panning/volume loops use `active_channels`
- Readers/converters -- populate correct number of channels
- Backend device code -- pass correct frame size to hardware

### Pros

- Genuinely supports quad, 5.1, 7.1 without API lies
- `std::array` is stack-allocated, still cache-friendly, no heap alloc
- Named accessors (`left()`, `right()`) keep call sites readable
- Future-proofs for spatial audio and bus routing

### Cons

- Large diff across ~28 files -- high regression risk
- `sizeof(AudioFrame)` grows from 8/16 bytes to 64+ bytes (8x Sample), increasing cache pressure in hot paths (mixer summing, SPSC queue)
- `active_channels` adds a runtime branch to every arithmetic operator
- Requires updating all readers, converters, and backends simultaneously -- hard to do incrementally

### Best for

If multi-channel output is a near-term requirement (e.g., Godot needs quad for VR audio within the next milestone).

---

## Solution C: Template on Channel Count

Make `AudioFrame` a template `AudioFrame<N>` with a compile-time channel count. Specialize the entire pipeline on the channel config.

```cpp
template <unsigned int N>
struct AudioFrame {
    static constexpr unsigned int CHANNEL_COUNT = N;
    std::array<Sample, N> channels{};
    _INLINE_ Sample &operator[](int idx) { return channels[idx]; }
    // operators all loop over N at compile time
};

using StereoFrame = AudioFrame<2>;
using QuadFrame   = AudioFrame<4>;
```

### Changes

- `lowl_audio_frame.h` -- template rewrite
- `AudioSource`, `AudioData`, `AudioMixer`, `AudioSpace` -- all become templates or use type-erased base + concrete instantiations
- Readers produce the correctly-typed frame
- Backends dispatch on channel count

### Pros

- Zero-overhead abstraction -- the compiler unrolls all channel loops
- `sizeof(AudioFrame<2>)` stays identical to today's struct (no bloat for stereo)
- Type-safe: can't accidentally mix a `QuadFrame` into a `StereoFrame` pipeline without an explicit conversion step

### Cons

- Enormous refactor -- templates propagate through the entire `AudioSource` hierarchy, reader/converter interfaces, and backend callbacks
- Template instantiation increases compile time and binary size
- `AudioMixer` mixing sources of different channel counts requires type erasure or runtime dispatch anyway, negating much of the type-safety benefit
- Hardest to do incrementally -- essentially a rewrite of the core pipeline
- Over-engineered for a library that currently only needs stereo

### Best for

A from-scratch design or a major version bump where breaking every interface is acceptable. Not practical as an incremental fix.

---

## Solution D: Keep Device Metadata, Reject >2 Channels in the Data Path

Keep `AudioChannel` and `AudioChannelMask` wide enough to describe backend/device capabilities, but make the frame-processing pipeline explicitly mono/stereo-only for now.

This means:

- keep `AudioChannel::Quadraphonic` and the wider `AudioChannelMask` values so WASAPI/CoreAudio can still report real device layouts
- add a small helper such as `supports_audio_frame_channel(AudioChannel)` that returns true only for `Mono` and `Stereo`
- reject unsupported channel counts at the points where audio enters the `AudioFrame` pipeline: readers, `AudioData`, `AudioStream`, and device `start()` / property selection
- treat `Quadraphonic` and higher as "known but unsupported" rather than pretending they are implemented

### Shape

```cpp
_INLINE_ bool supports_audio_frame_channel(AudioChannel channel) {
    switch (channel) {
        case AudioChannel::Mono:
        case AudioChannel::Stereo:
            return true;
        case AudioChannel::None:
        case AudioChannel::Quadraphonic:
        default:
            return false;
    }
}
```

Then use it to fail fast:

- readers set `UnsupportedAudioFormat` instead of returning misleading frames
- backend/device startup rejects `AudioDeviceProperties.channel > Stereo`
- `AudioSource::process_panning()` drops the fake `Quadraphonic` branch

### Changes

- `lowl_audio_channel.h` -- add an explicit helper for what the `AudioFrame` pipeline supports
- `lowl_audio_frame.h` -- document that the struct is a 2-channel frame type
- `lowl_audio_source.cpp` -- remove the fake `Quadraphonic` panning branch
- readers / device startup -- reject unsupported channels instead of silently misprocessing them
- optionally keep backend capability probing untouched so device metadata stays accurate

### Pros

- Fixes the false contract without throwing away useful device-capability metadata
- Smaller and safer than Solution B/C
- More honest than the current API, but less destructive than deleting enums and masks that backends already use
- Lets a future N-channel implementation reuse the existing capability model instead of rebuilding it

### Cons

- Still requires several runtime rejection points to be added consistently
- Public enums will still mention layouts the playback pipeline cannot yet handle
- Documentation must be explicit that wider layouts are probed metadata, not supported frame formats

### Best for

This codebase as it exists today: the backend layer already knows about wider channel masks, but the frame, mixer, and reader pipeline is still fundamentally stereo.

---

## Recommendation

**Go with Solution D.**

It keeps the part of the current design that is actually useful -- backend/device capability metadata -- while removing the dangerous lie that those layouts are already supported by `AudioFrame`. That makes it a better fit than Solution A as currently written, because Solution A throws away channel-mask metadata that WASAPI/CoreAudio already use for probing and format description.

If the goal is to stop silent corruption now without committing to a major refactor, Solution D is the best tradeoff:

- it is still a small, incremental fix
- it makes the frame pipeline honest
- it preserves future expansion room for a deliberate Solution B later
