# lowl_audio — Design Specification

**Directive date:** 2026-07-20

> **Implementation directive:** Use one `Mixer` node implementation and instantiate it as many times as the graph requires. `AudioBus` must not remain as a separate render or control implementation. A “bus” may exist as a user-facing name, handle, or convenience facade for a `Mixer` instance, but it must not duplicate mixing, connection, queue, handle, or acknowledgement machinery. New architecture work must converge on this model and must not extend the existing Bus/Mixer split.

## Core Concept

A composable, pull-model audio node graph. Nodes connect source→sink. The graph is evaluated from sinks backward on the render thread. All render-path evaluation is real-time safe (no locks, no allocations).

## Node Model

Every node can be read from (source) and/or written to (sink). Every node has built-in volume and panning — these are not separate nodes.

Adding a new node type = implementing one interface.

```
[Clip]   ──→ [Spatializer] ──→ [Mixer "SFX"]   ──→ [Mixer "Master"] ──→ [Device]
[Clip]   ──→ [Spatializer] ──↗
[Stream] ──→ [Reverb]      ──→ [Mixer "Music"]  ──↗
[Space]  ───────────────────────────────────────────↗
```

## Node Types

### Source Nodes (0 inputs → 1 output)

**Clip** — Fixed-length audio data with playback cursor. Data can originate from file, memory, or any source — once loaded it is just a sample buffer. Seek, loop, play/pause/stop/reset. Has volume, panning.

**Stream** — SPSC lock-free ring buffer. External producer writes interleaved or planar samples from any thread. Render thread consumes. Has volume, panning.

**Space** — Game audio manager. Preload assets into memory. Spawn playback instances, get handles back. Per-handle control: play, pause, stop, seek, reset, loop, volume, panning, query position/remaining. The Space itself is a source node with its own volume/panning that scales all its children.

### Processing Nodes (1 input → 1 output)

**Spatializer** — 3D audio positioning. Parameters: emitter position/velocity, listener position/orientation. Computes relative angle/distance and applies gain + panning. The library has no concept of a scene or shared listener — it is just math on two sets of coordinates set as parameters by the caller.

**Resampler** — Bridges different sample rates in the graph. Input-side AudioFormat has the source sample rate, output-side AudioFormat has the target sample rate. This is the only way to connect nodes with different sample rates.

**ChannelMap** — Bridges different channel layouts. Has a routing table: `source_channel[i] → output_channel[j]`. Unmapped output channels are filled with silence. Input-side AudioFormat has the source channel layout, output-side AudioFormat has the target channel layout. For complex routing (e.g. multiple sources with different layouts into a surround mix), use one ChannelMap per source to route into the target layout, then a Mixer to sum.

```
[SrcA Stereo] → [ChannelMap: L→bass, R→center, rest=silence]  → [Mixer 5.1] → [Device]
[SrcB Mono]   → [ChannelMap: mono→left, rest=silence]          ──↗
```

**Effects/DSP** — Extensible slot for user-defined processing (reverb, EQ, filters, compression, etc.). Architecture supports them; not all implemented day one.

### Mixing Nodes (N inputs → 1 output)

**Mixer** — Combines N sources. Per-input gain. Add/remove sources from the control thread, lock-free on the render thread. Has its own volume/panning.

There is one Mixer implementation and any number of Mixer instances. This does not mean one global mixer. Logical volume groups are ordinary Mixer instances: an “SFX” mixer and a “Music” mixer can both feed a “Master” mixer. A Mixer can therefore consume another Mixer exactly as it consumes any other compatible source node.

“Bus” describes the role of a Mixer instance; it is not a separate node type. If compatibility or ergonomics require APIs such as `create_bus()` or `AudioBusHandle`, they must be thin facades over Mixer nodes and the generic graph connection model. They must not introduce an `AudioBus` render class or a second control plane.

For gain-only hierarchy, Mixers propagate composed gain down to their inputs and those inputs accumulate directly into the downstream output block. A Mixer does not allocate, clear, or copy through an intermediate audio buffer merely because it is nested. Intermediate buffers are introduced only by processing that requires them, such as effects, resampling, channel mapping, or deliberate render caching.

Graph or controller code owns connection lifetime, mutation ordering, and user-facing handles. Ownership differences between callers must not produce different Mixer implementations. The render-side Mixer owns only the fixed-capacity input state needed to sum its active inputs safely.

### Sink Nodes (1 input → 0 outputs)

**Device** — OS audio backend (CoreAudio, WASAPI). Format conversion (float32/int16/int24/int32). Mono and stereo. Enumerate and select device properties. Real-time safe callback.

## AudioFormat and Validation

**AudioFormat** = `sample_rate` + `channel_layout`. Every node has an AudioFormat. This is the unit of compatibility inside the graph.

The format is passed as one `AudioFormat` constructor argument. Do not add parallel `(SampleRate, ChannelLayout)` constructor overloads; callers construct the value explicitly at the boundary.

Format-bearing types expose the complete AudioFormat rather than separate sample-rate or channel-layout accessors. `get_channel_count()` remains as a derived buffer-iteration convenience; it does not represent or store a second format.

The format vocabulary is deliberately non-overlapping:

- **FileFormat** selects an encoded file/container reader: WAV, MP3, FLAC, OGG, or Opus.
- **SampleFormat** describes the scalar representation at an import or device boundary: integer or floating point, with a defined bit width.
- **AudioFormat** describes graph compatibility only: sample rate and channel layout.
- **AudioDeviceProperties** stores an AudioFormat and adds `sample_format`, `exclusive_mode`, and backend-specific fields such as WASAPI valid bits per sample.
- Decoder-specific identifiers such as a WAV format tag remain private to that decoder.

There is no public `EncodedAudioFormat` type. Container/codec identity is discarded after decoding, and SampleFormat does not travel through the graph.

```
encoded bytes + FileFormat
        ↓ decoder
AudioData { AudioFormat, planar Sample[] }
        ↓ graph
Device { AudioDeviceProperties, boundary SampleFormat conversion }
```

The historical encoded-format enum named `AudioFormat` is removed. The name `AudioFormat` is reserved exclusively for the graph value type.

**Connection rule: AudioFormat must match on both ends of every connection.** No implicit conversion. If sample rates differ, insert a Resampler. If channel layouts differ, insert a ChannelMap.

**Format-converting nodes** (Resampler, ChannelMap) are the only nodes with different AudioFormats on input vs output. Validation checks that the input side matches the upstream node and the output side matches the downstream node. All other nodes have a single AudioFormat — input and output are the same.

**SampleFormat** is irrelevant inside the graph. All processing uses `Sample`. Format conversion (int16/int24/int32) only happens at boundaries: Device output and data import.

## Panning

Panning is gain scaling within the node's own channel layout. A stereo node pans across left/right. A mono node with panning just scales its single channel. No implicit upmixing — to go from mono to stereo, use a ChannelMap node.

## Internal Audio Format

All processing uses a single sample type: `float32` by default, `double` when compiled with `LOWL_TYPE_SAMPLE_64`. Controlled via the `Sample` typedef (`lowl_typedef.h`). Must be `atomic`-lock-free (static-asserted). Device node handles conversion to output formats (int16/int24/int32) at the boundary.

## Cross-Cutting Requirements

**Handle system** — All user-facing references are opaque handles with generation counters. No raw pointers in the public API.

**Real-time safety** — Render path: no locks, no allocations. Control-thread mutations (add/remove nodes, change parameters) are communicated via lock-free queues or atomics.

**Thread model** — Control thread (mutates graph, manages parameters), render thread (evaluates graph), producer threads (feed streams). No contention on render thread.

**Built-in gain/pan** — Every source node has volume and panning built in. Gain is applied inline during mix (one pass, no extra buffer copies). Separate processing nodes are reserved for actual effects.

## Scope

Implementation only. No tests, benchmarks, or test harnesses until the core library is complete.

## Decisions

- Gain/pan is built into every source node, not separate graph nodes. Reason: every source needs it, and inline application avoids extra buffer copies.
- Panning is gain scaling within the node's own channel layout. No implicit upmixing. Channel conversion is always explicit via a ChannelMap node.
- AudioFormat (sample_rate + channel_layout) must match on both ends of every connection. Format-converting nodes (Resampler, ChannelMap) are the bridges — they have different AudioFormats on input vs output.
- Format names have one responsibility: FileFormat identifies encoded input, SampleFormat identifies a boundary scalar representation, and AudioFormat identifies graph compatibility. Encoded file or decoder tags never enter the graph.
- One Mixer implementation, instantiated as many times as needed; not one global Mixer and not separate Bus + Mixer classes. The current split exists only because Space is single-owner while Mixer is multi-owner. That control-plane distinction does not justify duplicated render or connection machinery. The existing `AudioBus` implementation is transitional and must be removed rather than extended.
- Space is a source node in the graph, not a god object that owns the graph. It manages assets and playback handles. Graph topology is the caller's responsibility.
- Spatializer is a processing node, not part of Space. Listener and emitter are just parameters on the node, not shared state. The library has no concept of scenes or listeners.
- Clean rewrite over incremental refactor. The structural problems (Bus/Mixer duplication, AudioSpace as god object, double handle indirection) are load-bearing and not fixable incrementally. The proven lock-free primitives (ring buffer, event queues, gain caching) are portable to the new design.
