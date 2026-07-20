# lowl_audio — Design Specification

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

**Mixer** — Combines N sources. Per-input gain. Add/remove sources from control thread, lock-free on render thread. Has its own volume/panning. One implementation — no Bus/Mixer split. This is how volume groups work: a "SFX" mixer, a "Music" mixer, both feeding a "Master" mixer.

### Sink Nodes (1 input → 0 outputs)

**Device** — OS audio backend (CoreAudio, WASAPI). Format conversion (float32/int16/int24/int32). Mono and stereo. Enumerate and select device properties. Real-time safe callback.

## AudioFormat and Validation

**AudioFormat** = `sample_rate` + `channel_layout`. Every node has an AudioFormat. This is the unit of compatibility inside the graph.

**AudioDeviceProperties** is separate from AudioFormat. It adds `sample_format`, `exclusive_mode`, and backend-specific fields (e.g. WASAPI valid bits per sample). These only matter at the Device boundary — the Device derives its AudioFormat from the AudioDeviceProperties it negotiates with the OS.

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
- One Mixer class, not Bus + Mixer. The current split exists only because Space is single-owner vs Mixer being multi-owner. That distinction does not justify duplicated code.
- Space is a source node in the graph, not a god object that owns the graph. It manages assets and playback handles. Graph topology is the caller's responsibility.
- Spatializer is a processing node, not part of Space. Listener and emitter are just parameters on the node, not shared state. The library has no concept of scenes or listeners.
- Clean rewrite over incremental refactor. The structural problems (Bus/Mixer duplication, AudioSpace as god object, double handle indirection) are load-bearing and not fixable incrementally. The proven lock-free primitives (ring buffer, event queues, gain caching) are portable to the new design.
