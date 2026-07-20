# lowl_audio project status

Snapshot date: 2026-07-20

Repository state reviewed: branch `feat/new-1`, commit `ae753ce`, plus the full local working tree

## Executive summary

`lowl_audio` is a capable experimental C++ audio library, not yet a release-ready library. The decoding and playback foundation is substantial, the hot render path has received serious optimization work, and the current dirty tree can compile and pass its tests. The project is now blocked primarily by an unresolved architecture pivot rather than by a lack of implementation.

The most likely place work stopped was:

1. The direct-mix/SIMD hot-path rewrite was completed and committed on 2026-03-31.
2. A local follow-up then removed the last scratch-buffer fallback, introduced a purpose-built `AudioBus`, added bus-routed streams to `AudioSpace`, and expanded tests.
3. On 2026-04-01, `DESIGN.md` was rewritten to call for a clean, general node-graph architecture. That document explicitly rejects both the new Bus/Mixer split and an `AudioSpace`-owned graph.

In other words, the newest design thinking appears to supersede the uncommitted implementation that immediately preceded it. This is an inference from file history and content, but it explains the otherwise direct contradiction in the repository.

My recommendation is to preserve the current work as a checkpoint, stop extending the dedicated `AudioBus` design, and implement the new node architecture as a small tested vertical slice. Do not begin spatial audio, effects, Linux support, or more SIMD formats until the architecture, clean build, and real-time lifetime rules are settled.

## Repository and Git state

- Current branch: `feat/new-1`
- Upstream: `origin/feat/new-1`
- Local branch commits: synchronized with upstream (`+0/-0`)
- Compared with `master`: 67 commits ahead, 0 behind
- Last committed change: `ae753ce` on 2026-03-31
- `master` has not moved since 2023-09-22
- There are no release tags, project version, install rules, or exported CMake package.
- The branch is a very large change from `master`: about 23,000 additions and 6,000 deletions across 166 paths.
- The working tree is not clean: 24 tracked files are modified and 5 new source/header files are untracked.
- Raw working-tree diff: 1,537 additions and 1,099 deletions, excluding untracked files.
- Ignoring end-of-line churn, the tracked diff is closer to 901 additions and 463 deletions.

The new untracked files are the dedicated bus implementation and stream handle:

- `src/audio/source/lowl_audio_bus.cpp`
- `src/audio/source/lowl_audio_bus.h`
- `src/audio/source/lowl_audio_bus_event.h`
- `src/audio/source/lowl_audio_bus_slot_handle.h`
- `src/audio/source/lowl_audio_stream_handle.h`

The CoreAudio files were also converted to CRLF locally. `git diff --check` currently fails with extensive trailing-whitespace reports, and ordinary diffs make roughly 1,200 lines look changed when the meaningful CoreAudio change is only a few dozen lines. This should be cleaned in a separate mechanical commit, not mixed into architectural work.

The history is difficult to reconstruct because most of the 2026 commits are named only `up`. The code and documents carry much more useful intent than the commit messages.

## What exists today

The current implemented system is approximately:

```text
Lib
├── CoreAudio / WASAPI / Dummy drivers
│   └── AudioDevice
│       └── shared AudioSource render root
├── file readers: WAV, MP3, FLAC, OGG/Vorbis, Opus
└── AudioSpace
    ├── decoded, preconverted AudioData assets
    └── master AudioBus
        ├── child AudioBus nodes
        ├── AudioVoice clip playbacks
        └── AudioStream ring-buffer sources

AudioMixer remains as a separate lower-level multi-owner aggregator.
```

Important implementation characteristics:

- Internal rendering is planar `Sample` data, normally `float32`.
- Assets added to `AudioSpace` are decoded, resampled, and channel-converted off the render thread to the space's sample rate and layout.
- `AudioVoice`, `AudioStream`, `AudioMixer`, `AudioBus`, and `AudioSpace` directly accumulate through `mix_into()`.
- Gain and panning are cached and composed per channel, then applied in the accumulation loop.
- Hot buffers and clip data use 64-byte-aligned planar storage.
- Scalar, ARM NEON, and x86 AVX2/FMA kernels exist for scaled accumulation and common stereo output conversion.
- CoreAudio has a non-interleaved float32 direct-output path.
- WASAPI uses the generic planar render buffer followed by interleave/conversion.
- `AudioSpace` exposes generation-counted handles for assets, playbacks, buses, and—only in the dirty tree—streams.
- Structural mixer/bus edits are queued so the normal render loop does not take the control mutexes.

This is already a meaningful playback engine. It is much more than a skeleton, but it is still an evolving internal API with unresolved ownership and topology rules.

## Documentation and design decisions

### Decisions that are already reflected in code

- Decode, resample, and channel conversion are control-thread work, not callback work.
- The common hot path directly accumulates samples instead of rendering each voice through multiple scratch-buffer passes.
- Gain and panning are built into sources and buses.
- Format conversion is deferred to the device boundary where possible.
- Asset identity and playback-instance identity are separate.
- User-facing `AudioSpace` objects use owner IDs plus generation-counted handles to reject stale handles.
- Native backends target macOS/CoreAudio and Windows/WASAPI; the dummy backend supports tests and headless use.

### Decisions in the newest `DESIGN.md` that are not implemented

- A general pull-model node graph with source, processing, mixing, and sink nodes.
- A single `Mixer` abstraction instead of separate `AudioBus` and `AudioMixer` implementations.
- `AudioSpace` as an asset/playback source rather than the owner of graph topology.
- Explicit processing nodes for spatialization, resampling, channel mapping, and user DSP.
- A graph-level `AudioFormat` value containing sample rate and channel layout.
- A public API with no raw node pointers.
- Explicit connection validation and graph topology management.

There is also a naming collision to resolve before implementing that design: the existing `Lowl::Audio::AudioFormat` is a codec/encoding enum used by readers, while the new design uses the same name for sample rate plus channel layout. Rename the existing type or choose a different name for the graph format before public API work begins.

### Contradictory documents

- `follow_up.md` says the main rewrite is done and recommends introducing a purpose-built `Bus`; the dirty tree implements that recommendation.
- The newer `DESIGN.md` says one Mixer should replace Bus/Mixer duplication and calls for a clean rewrite.
- `assesment.md` describes the earlier flat `AudioSpace`/`AudioMixer` architecture and reports issues that the dirty tree has partly fixed, so it is no longer a current status report.
- `DESIGN.md` says to defer tests and benchmarks until the core is complete, despite the repository already having useful tests and this being concurrency/lifetime-sensitive code. This decision should be reversed: the rewrite should be test-driven from its first vertical slice.
- `README.md` describes the current bus-oriented API, not the newer proposed graph API.

The repository needs one authoritative design/roadmap and a small set of dated decision records. The historical optimization notes remain valuable, but they should be labeled as historical once their decisions have been incorporated or replaced.

## Validation performed for this report

### Build

A clean native arm64 configure succeeded with:

- CMake 4.3.4
- AppleClang 21
- Debug configuration
- benchmarks disabled

The normal clean build then failed because `-Werror` promotes this warning to an error:

```text
src/audio/convert/lowl_audio_sample_converter.h:45
implicit conversion from int to float changes 2147483647 to 2147483648
```

That file is unchanged by the current dirty patch. After downgrading only that diagnostic in a temporary build, the complete library, demo, and test executable compiled successfully.

The existing local `cmake-build-debug` directory is also stale and internally inconsistent: it targets `x86_64` while its Opus configuration enables ARM NEON, causing dependency compilation to fail. On this arm64 machine, build directories should be recreated or separated by architecture.

### Tests

With the single warning override, the current dirty tree produced:

```text
19 / 19 test cases passed
68,358 / 68,358 assertions passed
```

The runner covers named areas including audio data, devices, readers, sources, space, streams, converters, buffers, files, logging, and core utilities.

Limits of this result:

- No usable platform audio device was available in the test environment, so the CoreAudio initialization path was effectively not exercised end to end.
- Windows/WASAPI was not compiled or run on Windows during this review.
- The test runner exposes no dedicated top-level test names for `AudioBus`, `AudioMixer`, SIMD dispatch, queue saturation, or multithreaded graph mutation. These may have subcase coverage, but concurrency and capacity behavior need explicit stress tests.
- Benchmarks were not run because repository guidance says not to run them without explicit authorization.

## What is good

### Core functionality

- The library has a broad, useful feature base: five decoded file families, two native desktop backends, a dummy backend, buffered clips, streaming, mixing, and a high-level playback facade.
- Asset loading and conversion are correctly kept away from the audio callback.
- Existing playbacks retain shared clip data after an asset handle is removed, which gives sensible asset/playback lifetime semantics.
- Generation-counted handles and per-space owner IDs protect against stale or cross-space handle use.

### Render-path direction

- The direct `mix_into()` model substantially reduces clears, copies, and full-buffer passes.
- Cached per-channel gain avoids recomputing panning square roots every callback.
- Aligned planar storage and dedicated scalar/NEON/AVX2 kernels are appropriate foundations for a low-latency library.
- CoreAudio's direct non-interleaved float32 path is the correct fast path for macOS.
- The current WASAPI callback no longer performs the debug logging identified in the older assessment.

### Engineering foundations

- The build uses C++17, strict warnings, and hardening flags.
- Third-party libraries are pinned as submodules.
- The project has a substantial fast unit-test suite.
- The optimization documents explain the intended memory-pass reductions and make useful performance reasoning available to future work.

## What is bad or risky

### Critical: the implementation and the newest design disagree

Continuing to polish `AudioBus` would invest further in an architecture the newest design explicitly rejects. Conversely, starting a clean rewrite without checkpointing the working tree risks losing tested behavior and useful performance work. This decision must be made and recorded before feature development resumes.

### Critical: the dirty work is not safely checkpointed

The current bus/stream/lifetime work spans 29 paths and is only present locally. It also contains line-ending churn that obscures review. Preserve it on a named WIP branch or commit, then normalize line endings separately.

### High: a clean default build is red

The supported local toolchain cannot build the project with its normal flags because of the `int32_to_float()` warning. Existing build caches are architecture-confused. A new contributor following `README.md` will not get a clean build on this machine.

### High: `AudioBus` uses an SPSC queue with multiple producers

`AudioBus::acks` is a `BoundedSpscQueue`, but acknowledgements are produced both by the render thread (`process_events()` and source completion) and by control-thread rejection paths in `submit()`/`remove()`. That violates the queue's single-producer contract and can corrupt or lose acknowledgements under concurrency. `AudioMixer` already uses an MPSC acknowledgement queue for the analogous topology.

Additionally, almost every `AudioBus` acknowledgement enqueue ignores failure. A lost terminal acknowledgement can leave a playback, stream, or bus permanently retiring, because the control side no longer knows it is safe to release the raw source pointer.

### High: strict real-time safety is not yet proven

The steady-state source loops are good, but the blanket “no locks, no allocations” claim is too strong today:

- Device callbacks acquire a `shared_ptr<RenderState>` through C++17 atomic shared-pointer functions. These operations are not guaranteed lock-free, and the callback can participate in reference-count destruction.
- SIMD dispatch is initialized by a function-local static on first use. If the first use is the first audio callback, guard initialization and CPU detection happen on the real-time thread.
- Mixer and bus callbacks drain their entire structural event queues. In the worst case this can process hundreds or thousands of events, each with linear slot scans, in one callback. It is allocation-free but not deadline-bounded.
- Sparse fixed arrays can force long scans after churn; the dense active-list idea documented in `imp_hot_path.md` has not been implemented.

These are architecture-level requirements for the rewrite, not optional micro-optimizations.

### High: CoreAudio edge cases remain

- If the published render state is missing, the callback returns without explicitly silencing supplied buffers.
- The direct non-interleaved float32 branch dereferences `published_state->audio_source` without checking it, while `start()` currently permits a null source.
- The local CoreAudio edit contains CRLF/trailing-whitespace churn and several declarations accidentally joined onto one line in `lowl_audio_core_audio_utilities.cpp`.

The current tests did not exercise a real CoreAudio device, so these paths need targeted backend tests or a controlled manual validation pass.

### High: overload and asynchronous failure semantics are weak

`AudioSpace` can return a valid bus or stream handle before the parent bus has accepted the queued submission. Capacity or queue failure is reported later through an internal acknowledgement, while public creation/control methods generally return only a handle or `void`. Under saturation, callers cannot reliably distinguish accepted, pending, rejected, or retired objects.

Define a public result/error model and make capacity limits explicit. Add tests for full source arrays, full event/ack queues, rapid pause/resume/destroy sequences, and nested-bus rejection.

### Medium: the new graph is mostly a specification

There are no spatializer, effect, graph resampler, or graph channel-map nodes. Current resampling and channel conversion are offline asset operations. There is no connection API, cycle policy, fan-out policy, render-thread affinity rule, or general graph ownership model.

The current low-level `AudioMixer`/`AudioBus` APIs still accept raw `AudioSource*`, despite the newest design's “no raw pointers in the public API” requirement. The code also does not enforce whether one stateful source may be attached to multiple mixers or rendered by multiple device threads.

### Medium: nominal 64-bit sample support is inconsistent

`DESIGN.md` says defining `LOWL_TYPE_SAMPLE_64` selects `double`, but CMake sets a normal variable named `LOWL_TYPE_SAMPLE_64` to false and never exposes or applies it as a compile definition. Several device/SIMD paths are hard-coded to `float`. Either make this a supported, compiled, tested configuration or remove the claim until it is implemented.

### Medium: cross-platform and release readiness are limited

- macOS is the only platform compiled in this review, and its real device path was unavailable.
- WASAPI changes need a Windows build and device test.
- Linux has no backend.
- There are no install/export rules, package metadata, semantic version, ABI policy, tags, or release process.
- `feat/new-1` is 67 commits ahead of an effectively dormant `master`, so there is no small integration path or reviewable release boundary.

### Medium: documentation and history are hard to trust

The root documents describe different generations of the architecture without status labels. Commit messages do not explain intent. This is a serious maintenance cost for concurrency-sensitive code, where lifetime contracts need to be explicit and durable.

## Recommended next direction

Use the newest node-graph design as the target, but do not perform an unbounded “big bang” rewrite. Preserve the current implementation as a reference and build a minimal end-to-end graph alongside it:

```text
Clip node -> one Mixer node -> Dummy sink
```

That first slice should prove the public handle model, format validation, ownership, command handoff, and direct `mix_into()` path. Only after it is green should the native devices, streams, format adapters, and `AudioSpace` facade move onto it.

The strongest parts of the current code should be reused: aligned planar buffers, SIMD kernels, generation handles, bounded queues, immutable clip data, offline decode, and gain caching. The duplicate Bus/Mixer/Space bookkeeping should not be carried forward unchanged.

Tests must accompany each slice. For this project, postponing tests until “the core is complete” would make lifetime and real-time regressions much harder to isolate.

## Ordered task list

### P0 — decide, preserve, and restore a trustworthy baseline

1. **Checkpoint the dirty tree.** Create a clearly named WIP branch/commit containing the current bus/stream work and updated documents. Do not discard it; it is compiling, tested reference material.
2. **Separate line-ending cleanup.** Add an explicit `.gitattributes` policy, normalize the two CoreAudio files, run `git diff --check`, and keep this mechanical change out of architecture commits.
3. **Write one architecture decision record.** Confirm whether `DESIGN.md` is the intended target. Define node ownership, single- versus multi-sink attachment, graph mutation rules, cycle/fan-out policy, render-thread affinity, error reporting, and the exact role of `AudioSpace`.
4. **Resolve type/API names.** Rename the existing codec `AudioFormat` or the proposed graph format before either becomes a stable public API.
5. **Restore a clean default build.** Fix the `int32_to_float()` constant/conversion warning, recreate per-architecture build directories, and verify a no-override Debug and Release build from the documented commands.
6. **Make the current safety bugs explicit regression tests.** Cover multi-producer acknowledgements, queue overflow, source-capacity rejection, nested-bus rejection, null device sources, zero-frame callbacks, and rapid play/pause/resume/destroy sequences.

### P1 — prove the new core with a narrow vertical slice

7. **Define the minimal node and format contracts.** Keep the interface small: render/mix entry point, input/output format, lifetime/ownership token, and control-to-render synchronization. Do not add spatialization or effects yet.
8. **Implement Clip -> Mixer -> Dummy sink.** Use direct accumulation, cached gain/pan, immutable aligned clip data, generation handles, and one unambiguous retirement acknowledgement path.
9. **Make render-thread initialization explicit.** Initialize SIMD dispatch and other one-time state on the control thread. Replace or redesign callback-side atomic `shared_ptr` publication so the strict RT contract is credible.
10. **Bound callback work.** Put a per-block budget on structural commands, use dense active child storage or O(1) lookup where practical, and specify what happens when control queues are full.
11. **Add sanitizers and concurrency stress tests.** Run ASan/UBSan routinely and TSAN where supported. Include deterministic producer/render/control thread tests and destruction races.
12. **Port the native sinks.** Move CoreAudio first, then WASAPI, retaining float32-first negotiation and targeted fallback conversion. Validate each on real hardware before claiming backend support.

### P2 — complete useful composition

13. **Add Stream nodes.** Preserve the SPSC ring-buffer design, but expose producer ownership and backpressure clearly in the public API.
14. **Add explicit ChannelMap and Resampler nodes.** Reuse existing offline conversion code where appropriate, while keeping normal clip playback preconverted for speed.
15. **Rebuild `AudioSpace` as a facade.** It should manage assets and playback handles and optionally provide convenience wiring, without becoming the sole graph owner.
16. **Establish performance guardrails.** Once correctness and architecture are stable, run the existing benchmarks, refresh baselines, and add regression thresholds for the core render cases.

### P3 — release and expansion

17. **Consolidate documentation.** Update `README.md`, label historical design notes, replace `follow_up.md` with a live roadmap, and regenerate API docs.
18. **Add release engineering.** Introduce a version, install/export rules, a CMake package, supported-toolchain matrix, changelog, and a first pre-release tag.
19. **Only then add broader features.** Spatializer, DSP/effects, more SIMD output formats, Linux backend, and wider channel-layout work should follow a stable graph and measured demand.

## Immediate definition of “back on track”

The project is back on track when all of the following are true:

- The current WIP is safely checkpointed.
- One architecture document is authoritative and no longer contradicts the code being developed.
- A fresh documented Debug and Release build succeeds without warning overrides.
- `git diff --check` is clean.
- The bus acknowledgement topology and overflow/lifetime behavior are fixed or retired with the old architecture.
- The minimal new graph slice passes unit, sanitizer, concurrency, and dummy-sink tests.
- CoreAudio and WASAPI support claims are backed by platform builds and at least one real-device validation each.

Until those conditions are met, the honest project label is **advanced prototype / architecture transition**, not stable library.

## Review boundaries

Repository guidance explicitly disallows reading `doc/`, requires confirmation before reading test and benchmark sources, and forbids running benchmarks without an explicit request. This review therefore used the root documentation, all relevant `src/` implementation, Git history/diffs, clean builds, the compiled test runner, and its reported test list/results. Generated `doc/` content, test implementation details, benchmark implementation details, and benchmark results were not assessed.
