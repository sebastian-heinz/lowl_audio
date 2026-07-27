# lowl_audio — Design Specification

**Directive date:** 2026-07-24
**Status reviewed:** 2026-07-27

This document records the agreed architecture, component responsibilities, and boundaries of `lowl_audio`.
It describes the intended stable contract rather than serving as an implementation changelog.
Known gaps and planned extensions are listed separately under **Things Left To Do**.

## Goals

- Provide small audio components that can be composed explicitly.
- Use a pull-based render model in which an `AudioDevice` requests blocks from one root `AudioSource`.
- Make every renderable component follow the same `AudioSource` contract.
- Reuse one `AudioMixer` implementation for root mixes, nested submixes, logical volume groups,
  and the private mix inside an `AudioSpace`.
- Make `AudioSpace` the convenient game-facing component for registered audio assets and polyphonic playback by ID.
- Offer `AudioGraph` as an optional owning composition component that makes topology and lifetime rules safer.
- Keep standalone `AudioSource`, `AudioMixer`, and `AudioSpace` instances fully usable without `AudioGraph`.
- Let the application choose and own its composition root.
- Make formats, conversion, topology, ownership, lifetime, and failure visible in the API.
- Keep render-thread work deterministic, bounded, allocation-free, non-blocking, and free of control-side locks.
- Use fixed capacities and preallocated storage where they simplify real-time invariants.
- Preserve stale-handle safety without silently wrapping identities or generations into reuse.
- Perform decoding and required `AudioSpace` format conversion before audio reaches the live render path.
- Keep platform-specific behavior at the device and import boundaries rather than leaking it into graph components.

## Non-goals

### Architectural non-goals

- The library does not own a game scene, entity hierarchy, global listener, or spatial world.
- The library does not provide a mandatory or global `AudioGraph`.
- The library does not provide an `AudioBus`, `AudioBusHandle`, `create_bus()`, or a second bus-specific render path.
- The library does not perform implicit routing or implicit live-graph format conversion.
- Standalone `AudioMixer` does not own sources or validate topology; those guarantees belong to optional `AudioGraph`.
- A mixer is not a shared multi-owner control domain; it has one logical controller and acknowledgement consumer.
- A stateful source is not intended to be rendered concurrently by independent sinks.
- Connection-local gain is not part of the core contract; source gain and mixer gain cover the current model.
- Decoded assets are not graph nodes; asset data and playback state remain separate components.
- There is no universal scratch-buffer size or mixer scratch-capacity setting.
- Render callbacks do not mutate ownership trees, allocate memory, block, throw, or acquire control-side mutexes.

### Not part of the current contract

- `AudioVoice` and `AudioSpace` looping are deferred until non-looping lifecycle behavior is stable.
- Live `Resampler`, `ChannelMap`, `Spatializer`, and effects nodes remain planned explicit components.
- Per-playback processing inside `AudioSpace` remains unresolved; it will not be hidden behind an implicit bus model.
- WAV A-law, mu-law, and ADPCM variants are outside the current PCM/IEEE-float decoder contract.

## Component Purposes

- `AudioSource`
  - Purpose: common base for anything that can be pulled to render audio.
  - Provides one output `AudioFormat`, volume, panning, playback enable state, and `mix_into()`.
  - Does not know its render parent and does not own topology.

- `AudioBlockView` and `AudioBuffer`
  - Purpose: represent planar render storage without transferring ownership through the render graph.
  - `AudioBuffer` owns aligned storage; `AudioBlockView` is the bounded non-owning view passed to sources.
  - The current block contract supports up to eight channels.

- `AudioData`
  - Purpose: own decoded, render-ready planar sample data and its `AudioFormat`.
  - Represents an asset, not a graph node and not a playback cursor.
  - Its sample storage may be shared read-only by multiple voices.

- `AudioVoice`
  - Purpose: provide one finite playback instance over shared `AudioData`.
  - Owns independent position, playback state, volume, and panning.
  - Supports restart, pause, resume, stop, reset, seek, and playback queries.
  - Does not own or publish mixer attachment state.

- `AudioStream`
  - Purpose: bridge a live external producer into the pull-based render graph.
  - Uses a preallocated single-producer/single-consumer ring buffer.
  - Empty input is temporary starvation, not terminal completion.

- `AudioMixer`
  - Purpose: combine multiple format-compatible `AudioSource` instances into one source.
  - Is deliberately non-owning and uses generation-safe handles for fixed-capacity connections.
  - Supports manual composition when the caller manages topology and source lifetime correctly.
  - Uses the same implementation for roots, submixes, and the private mixer inside a Space.

- `AudioSpace`
  - Purpose: provide a game-facing sound bank and managed polyphonic voice source.
  - Owns registered `AudioData`, managed `AudioVoice` instances, and exactly one private `AudioMixer`.
  - Converts registered assets to the Space format during control-time preprocessing.
  - Exposes asset and playback handles instead of voice ownership.
  - Does not expose its private mixer, create submixes, own streams, or represent a scene/listener.

- `AudioGraph`
  - Purpose: safely own and compose an explicit source topology when manual lifetime management is undesirable.
  - Owns one active render tree plus detached subtrees and contains a permanent root mixer.
  - Enforces graph ownership, format compatibility, mixer-only parents, topology depth,
    and acknowledgement-driven teardown.
  - Is itself an `AudioSource` and remains optional; it does not replace standalone components.

- `AudioReader` and offline converters
  - Purpose: turn encoded input into `AudioData` and perform control-time resampling/channel conversion.
  - Decoder/container details stop at this boundary and are not part of graph compatibility.

- `AudioDevice`
  - Purpose: act as the sink boundary between one root `AudioSource` and a platform backend.
  - Owns a preallocated render/conversion buffer and converts planar `Sample` data to the selected
    device `SampleFormat`.
  - May render directly into backend-owned planar buffers when their scalar representation matches `Sample`.
  - Publishes immutable-for-the-run render state to the platform callback.

## Agreed Contracts and Decisions

### Render contract

- The device pulls one block from one root `AudioSource`.
- Any format-compatible source may be the root; an `AudioMixer` or `AudioGraph` is common but not required.
- Pulling nested mixers recursively renders their descendants on the same render thread.
- `mix_into()` accumulates into its destination and does not clear it.
- The sink clears the final block once before graph accumulation begins.
- Render storage is planar and uses the library `Sample` type.
- Every source has local volume and panning; upstream and local gain vectors compose during rendering.
- Panning changes channel gain inside the existing layout and never changes `AudioFormat`.
- A render result contains a produced-frame count and one state:
  - `Ok`: audio was produced normally.
  - `Starved`: a live source temporarily has no audio.
  - `Finished`: the source currently has no more output but does not request automatic detachment.
  - `Remove`: a finite source reached a terminal point and asks its mixer to detach it.
  - `Error`: rendering failed; already-produced audio may remain valid.
- Pausing an aggregate suppresses audio, not lifecycle work.
  - A paused `AudioMixer` still processes connection events and disconnection requests.
  - A paused `AudioSpace` pumps its private mixer with a zero-frame block.
  - A paused `AudioGraph` pumps its root mixer with a zero-frame block.
  - Lifecycle pumping requires the aggregate to continue being pulled.

### Composition, ownership, and topology

- Applications choose either manual composition or optional `AudioGraph` ownership.
- Manual composition uses ordinary `AudioMixer` instances and caller-owned sources.
- `AudioGraph` uses the same `AudioMixer` and render path; it adds ownership and topology validation only.
- A source with mutable playback state must have one render parent and one render thread.
- Each mixer has one logical control owner and one completion consumer.
- Different control threads may own different mixer instances that feed a common parent.
- Mutating a parent mixer's input list belongs to that parent's controller.
- Standalone mixer connections carry non-owning `AudioSource*` values.
- The caller keeps a manually connected source alive until its terminal completion is collected,
  or until quiescent mixer shutdown has completed.
- Graph-owned mixers must not be mutated through the low-level connection API because that bypasses graph bookkeeping.
- `AudioGraph` accepts detached nodes and connects detached subtree roots to render-reachable mixer nodes.
- `AudioGraph` supports at most 1024 owned nodes and a topology depth of at most 64.
- Disconnecting a graph node preserves its complete subtree; destroying an active node waits for render acknowledgement.
- Exactly one control thread mutates an `AudioGraph`, and exactly one render thread renders it.

### Mixer connection lifetime

- A mixer has 1024 fixed render/control connection slots.
- Connect commands cross to the render thread through a bounded queue.
- Disconnection requests use per-slot atomic flags.
- Connection failures caused by format, capacity, shutdown, or invalid input are synchronous.
- Terminal asynchronous completions are only `Removed` and `Finished`; there is no `Rejected` completion.
- A connection handle is one-shot and remains reserved until its terminal completion is collected.
- A reserved slot can produce at most one terminal completion before collection.
- The completion queue has the same capacity as the slot table, so valid state bounds outstanding completions.
- Completion enqueue failure is an invariant violation rather than expected backpressure.
  The render slot first enters a non-rendering `CompletionPending` state and retains the completion for retry,
  so even an invariant failure cannot silently orphan the caller-owned source lifetime.

### Playback and publication

- The playback model is an `AudioData` asset plus an `AudioVoice` playback instance.
- `AudioVoice::PlaybackSnapshot` contains only frame position and playback state.
- Mixer attachment/detachment belongs to the composition owner and is not intrinsic voice state.
- The snapshot uses one lock-free 64-bit atomic:
  - Bits 0–61 contain frame position.
  - Bits 62–63 contain playback state.
- The position field covers every frame index that can fit in one in-memory `Sample` allocation.
- Decisions involving both position and state read one snapshot rather than separate convenience getters.
- Voice control transitions are serialized by a control-side mutex that the render thread never takes.
- `AudioSpace` stores mixer handles and retiring state in its playback slots under `state_mutex`.
- Pausing or stopping a managed playback keeps its fixed mixer connection reserved.
- Destroying a connected playback requests detachment and releases the voice only after acknowledgement.
- Removing a Space asset prevents future lookup while existing voices may retain shared sample data.

### Identity and handles

- Zero is invalid for every handle identity component.
- `AudioAssetHandle` identifies registered data inside one `AudioSpace`.
- `AudioPlaybackHandle` identifies one managed voice inside one `AudioSpace`.
- `AudioMixerHandle` identifies one connection lifetime inside one `AudioMixer`.
- `AudioNodeHandle` identifies one owned node inside one `AudioGraph`.
- Asset, playback, and mixer handles contain:
  - 32-bit owner/instance identity.
  - 16-bit slot identity.
  - 32-bit generation.
  - Naturally aligned size of 12 bytes.
- Asset and playback tables can represent 65,535 nonzero slot IDs.
- A mixer uses 1024 of the 65,535 representable connection IDs.
- Slot generations advance on recycling; a slot retires permanently instead of wrapping into stale-handle aliasing.
- `AudioNodeHandle` contains a 32-bit graph identity and a 32-bit monotonic node identity.
- Node handles are 8 bytes and need no generation because graph node IDs are never recycled.
- Process-wide component identity exhaustion is fatal.
- Graph node-identity exhaustion returns an error.
- There is no `AudioBusHandle` or second handle indirection around a mixer.

### Formats and conversion

- `AudioFormat` is the compatibility value inside the render graph:
  - Sample rate.
  - Channel layout.
- `SampleFormat` describes scalar representation at an import or device boundary.
- `FileFormat` selects an encoded reader such as WAV, MP3, FLAC, OGG, or Opus.
- Decoder-specific tags remain private to the decoder.
- Container and codec identity are discarded after decoding into `AudioData`.
- Connected mixer/graph endpoints require equal sample rates and channel layouts.
- Live graph components do not resample or remap channels implicitly.
- `AudioSpace` explicitly preprocesses registered assets into its own format.
- Standalone callers may use the existing offline resampler and channel converter.
- Live format conversion will use explicit `Resampler` and `ChannelMap` source nodes.
- `Sample` is `float32` by default and `double` when `LOWL_TYPE_SAMPLE_64` is selected.
- Device-boundary conversion supports both `Sample` widths and every advertised output `SampleFormat`.
- PCM valid-bit precision is part of device-property identity, and samples are left-aligned when a backend
  uses fewer valid bits than its integer container width.
- Backend channel layouts use explicit speaker mappings; platform bit positions are never assumed to match
  the library `Speaker` mask.
- Matching planar float output may render directly into backend buffers; differing scalar widths use the
  preallocated render buffer and explicit conversion.
- Float32 retains the optimized SIMD write paths. Double `Sample` uses width-correct scalar conversion
  where a float32-only SIMD path would be invalid.

### Real-time, threading, and memory

- Render-thread code performs no heap allocation or deallocation.
- Render-thread code acquires no control-side mutex.
- Render-thread code performs no blocking system call.
- Render-thread code performs no `shared_ptr` reference-count change.
- Render-thread code performs no ownership-tree mutation.
- Render-thread work is bounded and uses preallocated storage.
- Invalid or unavailable device state produces silence rather than throwing.
- Control-side locks are valid for assets, playback management, names, handle tables, graph ownership, and devices.
- `AudioStream` has exactly one producer and one render consumer.
- Applications needing multiple producers combine them before one stream or give each producer a separate stream.
- `AudioMixer` applies queued connection work before rendering fixed source slots.
- `AudioGraph` changes ownership only on its control thread; rendering pulls its permanent root mixer.
- There is no universal scratch buffer.
- A processor owns only the preallocated intermediate/history storage its algorithm requires.
- Processor storage is sized during construction or a quiescent control operation, never in the render callback.

### Device boundary and lifecycle

- `AudioDevice` owns one immutable-for-the-run `RenderState` containing:
  - Selected `AudioDeviceProperties`.
  - Root-source ownership reference.
  - Preallocated render buffer.
- The control side owns `RenderState` with `unique_ptr` and publishes only a lock-free atomic raw pointer.
- The callback loads the pointer but does not copy the contained `shared_ptr`.
- Startup publishes complete state before callbacks may consume it.
- Shutdown order is:
  1. Unpublish render state.
  2. Stop the backend callback or audio thread.
  3. Wait for callback activity to become quiescent.
  4. Dispose backend resources.
  5. Release render state and root-source ownership.
- Backend stage flags change only after their corresponding lifecycle operation succeeds.
- A failed partial shutdown leaves enough stage state for a later `stop()` to resume cleanup.
- Platform failures are checked and logged at the failing call site, written to `Error`, and followed by early return.
- Exceptions are not used on the render path.

## Planned Component Extensions

- `Resampler`
  - Purpose: bridge different sample rates in a live graph.
  - Must preallocate render state and define latency, flush, starvation, and terminal propagation.

- `ChannelMap`
  - Purpose: bridge channel layouts with explicit routing, upmix, downmix, and silence rules.
  - May own a preallocated intermediate block when direct accumulation is insufficient.

- `Spatializer`
  - Purpose: consume caller-provided listener/emitter values and produce spatial gain/panning.
  - Will not own a scene or global listener.

- Effects and application-defined DSP
  - Purpose: provide explicit filters, EQ, dynamics, reverb, delay, and custom processing sources.
  - Must define scratch/history sizing, latency, bypass, reset, and terminal-state propagation.

- Processing nodes hold one upstream source.
- Format-converting processors expose an input format matching upstream and an output format matching downstream.
- Other processors preserve input/output format.
- Processing an entire `AudioSpace` is possible by placing a processor after it.
- Per-playback processing needs a separate Space extension because managed voices are intentionally hidden.

## Validation Policy

- Test correctness and real-time behavior alongside implementation changes.
- Keep mechanical repository changes separate from architectural or behavioral changes.
- Stress bounded queues, lifecycle acknowledgements, generation reuse, and cross-thread publication.
- Validate both manual composition and `AudioGraph` ownership paths.
- Validate device lifecycle failure and retry behavior on real CoreAudio and WASAPI hardware.
- Test callback sizes from backend-reported maxima rather than assuming common defaults.
- Measure the complete render path before accepting performance complexity.
- Do not weaken ownership, lifetime, or real-time guarantees for an unmeasured micro-optimization.

## Things Left To Do

All repository-verifiable P0 implementation and automated-test work described by this specification is complete.
The remaining P0 work requires physical backend environments; it is validation work rather than a known code gap.

### P0 — Correctness and Lifetime Safety

- Validate staged start/stop failure handling on a real CoreAudio output device.
  Exercise every lifecycle stage, retry `stop()`, and verify that callbacks cannot observe released state or memory.
  The current development environment did not expose a usable CoreAudio output device.
- Perform the equivalent validation on real WASAPI hardware when a Windows environment is available.
  Real WASAPI testing is intentionally outside the current pass.

### P1 — Explicit Processing Components

P1 is intentionally postponed until the remaining physical P0 validation is complete.

- Implement live `Resampler`.
- Implement live `ChannelMap`.
- Resolve per-playback processing inside `AudioSpace`, then implement `Spatializer`.
- Define and implement the effects/application-DSP source contract.

### P3 — Explicitly Deferred or Optional

- Define and add `AudioVoice`/`AudioSpace` looping after the non-looping lifecycle is stable.
- Add decoder coverage only when required.
  Unsupported WAV A-law, mu-law, and ADPCM variants must fail explicitly rather than decode as linear PCM.
