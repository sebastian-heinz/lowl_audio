# lowl_audio — Design Specification

**Directive date:** 2026-07-21

> **Implementation directive:** `lowl_audio` is an explicit, pull-based audio graph. Use one `AudioMixer` implementation and instantiate it as many times as the application needs. Do not introduce `AudioBus`, `AudioBusHandle`, `create_bus()`, or another bus-specific render/control path. An `AudioSpace` owns exactly one private `AudioMixer`, registers render-ready assets, and manages playback voices through IDs. The caller owns every mixer and connection outside a Space.

This document is the aligned architecture and implementation direction. Sections marked **Implemented** describe the current foundation. Sections marked **Planned** describe intended additions and are repeated in priority order in **Things Left To Do** at the bottom.

## Goals

- Provide small audio building blocks that can be composed explicitly.
- Make `AudioSpace` the convenient game-facing building block for preloaded sound assets and polyphonic playback by ID.
- Keep the render callback deterministic and real-time safe: no locks, allocations, blocking, or ownership churn.
- Make formats, conversion, topology, ownership, and failure visible rather than implicit.
- Reuse one mixer design for root mixes, submixes, logical volume groups, and the private mix inside a Space.

The library does not own a game scene, global listener, or application-wide audio graph. It supplies sources, mixers, import/conversion utilities, and device backends; the application composes them.

## Whole-System Picture

```text
encoded file / bytes
        |
        v
   AudioReader ---------------------> AudioData
   WAV, MP3, FLAC, OGG, Opus          decoded planar samples + AudioFormat
                                             |
                         register / convert  |
                                             v
                                      AudioSpace asset registry
                                             |
                                      create playback ID
                                             |
                                             v
                                        AudioVoice(s)
                                             |
                                      private AudioMixer
                                             |
                                             v
                                         AudioSpace --------------------+
                                                                        |
external producer --> AudioStream --> optional processing --> AudioMixer(s) --> AudioDevice
                                                                        ^
manually-owned AudioSource / AudioMixer --------------------------------+
```

The device pulls a block from its root `AudioSource`. A root source is commonly an `AudioMixer`, but it can be any format-compatible source. Pulling a parent mixer recursively pulls nested mixers and their sources on the same render thread.

There is no central `AudioGraph` object and no implicit routing. The graph is the object topology created by the caller.

## Core Render Contract — Implemented

`AudioSource` is the common renderable abstraction. `AudioVoice`, `AudioStream`, `AudioMixer`, and `AudioSpace` are all sources. Each source has one output `AudioFormat`, volume, panning, playback enable state, and a `mix_into()` implementation.

`mix_into()` accumulates samples into a planar `AudioBlockView`; it does not assume that it owns or should clear the destination. The sink clears the final block once, then the graph accumulates into it. This permits nested gain-only mixers to render directly into the device block without intermediate copies.

The render result contains a produced-frame count and one state:

- `Ok` — audio was produced normally.
- `Starved` — a live source temporarily had no audio, such as an empty stream.
- `Finished` — the source currently has no more output.
- `Remove` — a finite source reached a terminal point and its mixer should detach it.
- `Error` — rendering failed; callers may preserve any audio already produced while propagating the error state.

The current audio block supports up to eight channels. All render-time storage is planar and uses the library `Sample` type.

## Data, Voices, and Playback — Implemented

### `AudioData`

`AudioData` is decoded asset data, not a graph node and not a playback cursor. It owns aligned planar sample storage, a frame count, a name, and one `AudioFormat`. Sample storage is shared read-only by playback voices.

There is no separate `Clip` node in the design. Where old terminology says “clip,” the actual model is:

```text
AudioData (asset) + AudioVoice (playback instance)
```

### `AudioVoice`

An `AudioVoice` is a finite source backed by shared `AudioData`. Every voice has independent position, playback state, volume, and panning. It supports play/restart, pause/resume, stop, reset, and seek.

Playback position, playback state, and mixer-detachment state are published as one coherent `PlaybackSnapshot`. Code making a decision from more than one of these fields must read the snapshot once. Reading separate convenience getters is acceptable only when no cross-field invariant is required.

Control transitions are serialized by a control-side mutex. The render thread never takes that mutex; it consumes atomic commands and publishes the coherent snapshot atomically. This prevents impossible observations such as combining a new state with an old position while preserving real-time safety.

Looping is not part of the current voice contract. It is explicitly deferred.

### `AudioStream`

`AudioStream` is a live source backed by a preallocated single-producer/single-consumer ring buffer. One producer writes planar or interleaved `Sample` frames; the render thread consumes them. Empty input produces `Starved`, not terminal removal.

The SPSC contract is intentional: one stream has one producer and one render consumer. Applications needing multiple producers must combine them before the stream or give each producer its own stream and mix those streams.

## `AudioSpace` — Implemented

`AudioSpace` is the game-facing sound-bank and voice-management source. It exists so a game can register or preload audio once, prepare it for playback, and thereafter control playback through stable IDs rather than repeatedly decoding or directly owning voice objects.

Registering audio performs control-time work:

1. Decode a supported file or accept existing `AudioData`.
2. Resample it to the Space sample rate when necessary.
3. Convert it to the Space channel layout when necessary.
4. Store the render-ready asset and return an `AudioAssetHandle`.

This conversion is preprocessing at the Space boundary, before the asset enters the live graph. It is not implicit conversion between connected graph nodes.

Creating playback constructs an `AudioVoice` and returns an `AudioPlaybackHandle`. Per-playback operations include play, pause, resume, stop, destroy, seek, reset, volume, panning, and position/frame queries. Removing a registered asset prevents new lookups while existing voices may retain the shared sample data they need.

Every Space owns exactly one private `AudioMixer`. Its voices feed that mixer, and the Space delegates rendering to it. The Space itself is an `AudioSource`, so its own volume and panning scale the aggregate result.

A Space does not:

- expose or replace its private mixer;
- create buses or submixers;
- own `AudioStream` objects;
- own the root device graph;
- route itself into an external mixer;
- represent a scene or listener.

If an application wants several Spaces, a music stream, or manually-owned sources grouped together, it creates external `AudioMixer` instances and connects those sources explicitly.

## Mixing and Topology — Implemented

There is one `AudioMixer` class and any number of instances. “One mixer” means one implementation, not a singleton and not one instance for the whole application.

```text
AudioSpace "World SFX" ----> AudioMixer "SFX" -----+
AudioSpace "UI" -----------> AudioMixer "SFX"      |
                                                     v
AudioStream "Music" -------> AudioMixer "Music" -> AudioMixer "Master" -> AudioDevice
```

Names such as “SFX bus” or “music bus” are valid application terminology, but those objects remain ordinary mixers. The library does not need a bus type, bus handle, or `create_bus()` facade. A caller that needs a submix constructs another mixer and connects it to its parent like any other source.

Each source and mixer has its own gain/panning. A mixer's gain controls its aggregate output; a child source's gain controls that source. Connection-local/per-input gain is not part of the current core contract. Because a stateful source is expected to have one render parent, source and group gain cover the established use cases without another parameter ownership model.

Nested gain-only mixers do not need scratch buffers. They compose their gain with the upstream gain vector and let children accumulate directly into the downstream block. This avoids clear/copy passes for ordinary submixes.

The render-side mixer has bounded, preallocated source slots; the current limit is 1024 active sources per mixer. Add/remove commands cross to the render thread through a bounded queue. A terminal acknowledgement (`Removed`, `Finished`, or `Rejected`) crosses back to the controller.

### Mixer Ownership and Threads

Each mixer has one logical controller and one acknowledgement consumer. The mixer does not maintain several owner registries or route acknowledgements to multiple owners. Its control mutex serializes API bookkeeping, but that does not turn one mixer into a shared multi-owner control domain.

Different control threads may own and operate different mixer instances that feed a common parent. The parent still pulls the complete nested graph on one render thread. Mutations of the parent's input list belong to the parent's controller.

A stateful source must not be rendered concurrently by two independent sinks. Connecting a child mixer to one parent is supported; attaching the same child to two independently running device graphs is not.

### Connection Lifetime

Mixer connections use mixer-scoped, generation-safe `AudioMixerHandle` values. The low-level mixer connection currently carries a non-owning `AudioSource*`. The caller must keep the source alive until it receives the matching terminal acknowledgement, then release the handle. `AudioSpace` implements this retirement protocol internally for its voices.

The handle prevents stale commands from targeting a recycled slot; it does not own the source. This distinction must remain explicit in the API and documentation.

## Identity and Handles — Implemented

The three handle types have deliberately separate scopes:

- `AudioAssetHandle` identifies registered data inside one `AudioSpace`.
- `AudioPlaybackHandle` identifies one managed voice inside one `AudioSpace`.
- `AudioMixerHandle` identifies one connection slot inside one `AudioMixer`.

Each handle contains an owner/mixer identity, slot identity, and generation. Recycling a slot advances its generation so stale handles fail validation. There is no `AudioBusHandle` and no second handle indirection around a mixer.

## Audio Formats and Conversion — Implemented Foundation

`AudioFormat` is the graph compatibility value:

```text
AudioFormat = sample_rate + channel_layout
```

Format-bearing types store and expose the complete `AudioFormat`. Constructors accept one `AudioFormat` value; parallel `(SampleRate, ChannelLayout)` constructors are not provided. `get_channel_count()` remains as the only derived convenience accessor because it is frequently needed for buffer iteration. Sample rate and channel layout are read from `get_audio_format()` and are not stored a second time.

`AudioDeviceProperties` also contains an `AudioFormat`, then adds boundary information such as `SampleFormat`, exclusive mode, support state, and backend-specific fields.

The format vocabulary has one responsibility per type:

- `FileFormat` selects an encoded reader: WAV, MP3, FLAC, OGG, or Opus.
- `SampleFormat` describes scalar representation at an import or device boundary.
- `AudioFormat` describes compatibility inside the graph.
- Decoder-specific tags, such as WAV codec tags, stay private to the decoder.

There is no public encoded-audio-format bundle. Container and codec identity are discarded after decoding.

```text
encoded bytes + FileFormat
        |
        v
AudioData { AudioFormat, planar Sample[] }
        |
        v
graph of AudioSource objects
        |
        v
AudioDevice { AudioDeviceProperties, boundary SampleFormat conversion }
```

Connected graph endpoints must have equal sample rates and channel layouts. There is no implicit live-graph conversion. The mixer rejects mismatched sources.

Offline resampling and channel conversion already exist and are used while registering assets in a Space. Live `Resampler` and `ChannelMap` processing nodes are planned; until they exist, live sources must already match their destination format.

### Internal Sample Type and Panning

All graph processing uses `Sample`: `float32` by default or `double` with `LOWL_TYPE_SAMPLE_64`. Import converts encoded sample representations into `Sample`; the device converts `Sample` to its selected output `SampleFormat`.

Panning is channel gain inside the source's existing layout. It never changes `AudioFormat`. Stereo pans across the left/right channels; mono panning only scales its one channel. Upmixing, downmixing, and arbitrary routing are channel conversion, not panning.

## Planned Processing Nodes

Processing nodes will implement `AudioSource` while holding one upstream source. They are explicit graph objects rather than modes hidden in a mixer or Space.

- `Resampler` bridges different sample rates in a live graph.
- `ChannelMap` bridges channel layouts through an explicit routing/downmix/upmix policy.
- `Spatializer` accepts caller-provided listener and emitter values and computes spatial gain/panning; the library does not own a scene or global listener.
- Effects/DSP nodes cover filters, EQ, dynamics, reverb, delay, and application-defined processing.

A format-converting processor has an input format matching its upstream source and a different output format matching its downstream sink. Other processors have the same input and output format.

Per-playback processing inside `AudioSpace` needs an explicit extension design because Space intentionally owns and hides its voices. Processing the Space as a whole is already possible by placing a processor after it; inserting a unique processor between each managed voice and the private mixer is not yet represented by the public API.

## Real-Time and Memory Model — Implemented Foundation

The render path obeys these rules:

- no mutex acquisition;
- no heap allocation or deallocation;
- no blocking system calls;
- no `shared_ptr` reference-count changes;
- no graph mutation in place;
- bounded work and preallocated storage;
- invalid or unavailable device state produces silence rather than throwing.

Control-side locks are valid for asset registration, playback management, names, handle tables, and device lifecycle. Atomics or bounded queues publish only the state needed by rendering.

`AudioSpace::state_mutex` is control-side only. `AudioVoice` uses atomic render commands and a coherent atomic snapshot. `AudioStream` separates its producer and consumer cursors. `AudioMixer` applies queued mutations before rendering a block and reads from fixed source slots.

## Buffers and Scratch Storage — Implemented Policy

There is no universal `DefaultScratchBufferCapacity` and no mixer scratch-capacity parameter.

The device owns the final planar render buffer. Its frame capacity comes from the backend's actual callback requirement—CoreAudio's maximum frames per slice or WASAPI's buffer size—not from an arbitrary library constant.

Simple sources and nested mixers render directly into the downstream block. A processor that genuinely needs intermediate or historical data owns its own storage:

- delay/reverb owns persistent history;
- resampling owns filter state and any required staging;
- channel mapping may own a preallocated intermediate block when direct accumulation is insufficient;
- look-ahead or deliberately delayed processing owns a preallocated delay buffer.

Such storage is sized during construction or a quiescent control operation. It is never allocated or resized in the render callback.

## Device Boundary and Lifecycle — Implemented Foundation

`AudioDevice` is the sink boundary. CoreAudio and WASAPI enumerate/select `AudioDeviceProperties`, run their platform callback, pull one root source, and convert the planar internal block into the selected device sample representation. The dummy backend supports non-hardware use.

The callback reads one immutable-for-the-run `RenderState` containing:

- selected `AudioDeviceProperties`;
- the root source ownership reference;
- the preallocated render buffer.

The control side owns this state with `unique_ptr` and publishes only a lock-free atomic raw pointer. The callback loads the pointer but never copies the contained `shared_ptr`. Startup publishes a complete state before callbacks may use it. Shutdown follows this ordering:

1. Unpublish the render state so new callback work cannot acquire it.
2. Stop the backend callback/audio thread.
3. Wait until backend callback activity is quiescent.
4. Dispose backend resources.
5. Release the unpublished render state and root source ownership.

Backend started/initialized/listener flags are changed only after a lifecycle stage succeeds. If shutdown fails partway through, a later `stop()` can resume from the remaining stage rather than assuming everything was disposed.

Device lifecycle code is intentionally top-down and explicit. Each failing platform call is checked at the call site, logged with context, written to `Error`, and followed by an early return. Cleanup is not hidden behind a generic result accumulator. Programmer-invariant guards log, assert in debug builds, and also return a failure in release builds. Exceptions are not used on the render path.

## Decision Record

- One `AudioMixer` implementation, many mixer instances; no global mixer singleton.
- No `AudioBus`, bus handle, bus factory, or bus-specific render path.
- One private mixer per `AudioSpace`; all external topology belongs to the caller.
- `AudioSpace` is a renderable sound bank and voice manager, not a graph or scene owner.
- `AudioData` stores decoded samples; `AudioVoice` stores playback state. There is no separate Clip node.
- `AudioFormat` is one bundled graph value and the single constructor argument for format-bearing objects.
- Only `get_channel_count()` remains as a format convenience; sample rate and layout come from `get_audio_format()`.
- Graph connections require exact `AudioFormat` compatibility; conversion is explicit.
- Gain/panning is built into every source; mixer gain controls a group. No connection-local gain is required by the current design.
- A mixer has one logical controller and acknowledgement owner. Different mixers may have different control threads.
- Nested gain-only mixers render without scratch buffers. Processors own only the preallocated state they require.
- Multi-field playback observations use one coherent `PlaybackSnapshot`.
- Device callbacks consume an atomically published, preallocated render state and never take control-side ownership.
- Lifecycle error handling uses visible call-site checks, logging, `Error`, and early return.
- Looping is deferred.

## Validation Policy

Correctness and real-time behavior must be tested alongside implementation. Mechanical repository changes, such as line-ending normalization, stay in separate commits from architecture or behavior changes. Performance work should measure the complete render path and must not weaken ownership, lifetime, or real-time guarantees for unmeasured micro-optimizations.

## Things Left To Do

### P0 — Correctness and Lifetime Safety

1. Make terminal mixer acknowledgements loss-proof under queue saturation. A dropped `Removed`, `Finished`, or `Rejected` acknowledgement must never leave a source lifetime permanently unresolved; add overflow recovery and stress coverage.
2. Validate staged start/stop failure handling on real CoreAudio and WASAPI devices. Exercise failure at every lifecycle stage, retry `stop()`, and verify that no callback can observe released `RenderState`, source, or buffer memory.
3. Add focused concurrency stress coverage for mixer add/remove/retire, Space playback-slot recycling, stale generations, and coherent `AudioVoice::PlaybackSnapshot` observations.

### P1 — Complete the Explicit Graph

4. Implement the live `Resampler` source node. Reuse the existing offline resampling work where appropriate, but preallocate all render state and define latency/flush behavior.
5. Implement the live `ChannelMap` source node with explicit routing, upmix, downmix, and silence rules.
6. Resolve the per-playback processing extension for `AudioSpace`, then implement `Spatializer`. The solution must preserve ID-based control, one private mixer, caller-owned external topology, and render-time lifetime safety without reintroducing AudioBus.
7. Define and implement the effects/DSP source contract, including construction-time scratch/history sizing, latency reporting where needed, bypass, reset, and terminal-state propagation.

### P2 — API and Platform Hardening

8. Harden or wrap the low-level non-owning mixer connection API so the source-until-ack lifetime rule is difficult to misuse, while keeping graph topology explicit.
9. Remove remaining public “clip” vocabulary, such as `play_clip()`, in favor of asset/voice/playback terminology, with a deliberate compatibility plan if the API is already consumed externally.
10. Verify advertised channel-layout and `SampleFormat` behavior across CoreAudio and WASAPI, including callback sizes larger than common defaults and layouts up to the graph's eight-channel limit.
11. Add small composition examples for a root mixer, nested mixers, multiple Spaces, a stream, and clean acknowledgement-driven teardown. The examples must not introduce a bus facade.

### P3 — Explicitly Deferred / Optional

12. Add `AudioVoice` and `AudioSpace` looping only after the core non-looping lifecycle is stable. Define loop bounds, seek/reset interaction, snapshot semantics, and terminal acknowledgement behavior before implementation.
13. Add further decoder coverage only when required. WAV A-law, mu-law, and ADPCM variants are currently outside the supported PCM/IEEE-float WAV path and should fail explicitly rather than be interpreted as linear PCM.
