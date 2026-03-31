# Follow-Up

These are the remaining items worth doing after the current hot-path rewrite.

## 1. Expose Stream And Generic Node Routing Through `AudioSpace`

Current state:

- `AudioSpace` now has a real bus tree for clip playbacks.
- It still does not expose first-class bus attachment for `AudioStream` or arbitrary external nodes.

Why it still matters:

- The stated end goal was standalone components that are composable, independently controllable, and easy to use from the outside.
- Right now, clip routing is easy through `AudioSpace`, but streaming/custom nodes still require manual `AudioMixer` wiring.

Concrete follow-up:

- Add `AudioStreamHandle` support to `AudioSpace`.
- Add APIs to create/destroy routed streams on a target bus.
- Optionally add a generic `attach_source(AudioSource*, AudioBusHandle)` style API if external ownership is acceptable.

## 2. Remove The Legacy `render()` Compatibility Path From The Hot Graph

Current state:

- The common path uses direct `mix_into()`.
- The base `AudioSource::mix_into()` still supports legacy `render()` by rendering into scratch and then accumulating.

Why it still matters:

- That fallback keeps scratch-buffer semantics alive in the architecture.
- It prevents the engine from being a fully enforced direct-accumulate graph.

Concrete follow-up:

- Make `mix_into()` the only required render entry point for graph nodes.
- Remove or isolate the scratch fallback behind an adapter node that is not used in the hot graph.
- Then drop unused device/mixer scratch usage on the fully direct path.

## 3. Replace `AudioMixer` Queue/Handle Machinery With A Purpose-Built `Bus` Node

Current state:

- Buses are implemented with nested `AudioMixer` instances.
- `AudioMixer` still carries event queues, ack owners, handle allocation, and source rebinding checks.

Why it still matters:

- This is functionally correct, but it is not the minimal architecture described in `imp_hot_path.md`.
- A dedicated `Bus` node would be simpler and cheaper than reusing the general mixer control plane everywhere.

Concrete follow-up:

- Introduce a bus-specific child container and control command path.
- Keep structural edits off the audio thread, but remove per-bus ack-owner/handle/event overhead where it is no longer needed.
- Retain `AudioMixer` only if a public low-level mixer type is still wanted.

## 4. Add More Typed SIMD Output Kernels

Current state:

- Hot mix accumulation has SIMD kernels.
- Stereo `FLOAT32` and stereo `INT16` device writes now have typed fast paths.
- Other output formats still use the generic scalar conversion writer.

Why it still matters:

- If a backend/device forces `INT24`, `INT32`, `FLOAT64`, or wider multichannel interleave, the engine drops back to a slower generic loop.

Concrete follow-up:

- Add typed kernels for the formats you actually care about in production.
- Prioritize `INT32` and `INT24` if WASAPI/device negotiation can surface them often.
- Add mono/stereo specializations before generic N-channel SIMD work.

## 5. Lock Backend Format Policy Down With Tests

Current state:

- CoreAudio non-interleaved `FLOAT32` is implemented.
- The generic device path prefers typed writes when the sample format matches.
- The intended “float32 first, convert only when forced” policy is not yet fully locked down by backend-specific tests.

Why it still matters:

- The architecture is only as strong as the actual negotiated backend format.
- Regressions here can silently reintroduce avoidable conversion cost.

Concrete follow-up:

- Add tests that assert CoreAudio keeps using non-interleaved `FLOAT32` where supported.
- Add tests that assert WASAPI prefers `FLOAT32` shared-mode formats when available.
- Add tests for fallback behavior when integer output is unavoidable.

## 6. Add Perf Regression Guardrails

Current state:

- The code builds, tests pass, and benchmark smoke runs succeed.
- There is no automated guard against hot-path regressions.

Why it still matters:

- The current design is performance-sensitive enough that small structural regressions can cost a lot.

Concrete follow-up:

- Add benchmark baselines for the main render cases.
- Add CI comparison thresholds for the hottest benchmarks.
- Optionally inspect generated assembly for the main SIMD kernels in CI or release validation.

## 7. Clean Minor Non-Hot-Path Backend Debt

Current state:

- There are still small backend TODOs and utility cleanup items outside the central render loop.

Why it still matters:

- Not a hot-path blocker, but worth clearing once the architecture is stable.

Concrete follow-up:

- Remove leftover backend TODOs.
- Tighten any format/layout negotiation code that is now redundant after the fixed-domain redesign.

## Bottom Line

The main render-path rewrite is done.

What remains is mostly:

- finishing the public composition surface for streams/custom nodes
- removing the last legacy compatibility layer
- simplifying bus internals further
- extending typed SIMD/device specialization
- locking performance and backend policy down with stronger tests/benchmarks
