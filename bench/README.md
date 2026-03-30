# Benchmarks

This directory contains opt-in microbenchmarks built with Google Benchmark.

## Configure

Use a dedicated Release build directory for benchmarks:

```bash
cmake -S . -B build-bench -DCMAKE_BUILD_TYPE=Release -DLOWL_BUILD_BENCHMARKS=ON
cmake --build build-bench --target lowl_audio_bench
```

## Run

Run the full benchmark suite:

```bash
./build-bench/bench/lowl_audio_bench
```

Run a subset:

```bash
./build-bench/bench/lowl_audio_bench --benchmark_filter=AudioDevice
```

Save machine-readable output:

```bash
./build-bench/bench/lowl_audio_bench \
  --benchmark_out=bench/results/current.json \
  --benchmark_out_format=json
```

Recommended flags for more stable comparisons:

```bash
./build-bench/bench/lowl_audio_bench \
  --benchmark_repetitions=10 \
  --benchmark_report_aggregates_only=true \
  --benchmark_min_time=0.1s \
  --benchmark_out=bench/results/current.json \
  --benchmark_out_format=json
```

## Baselines

Store intentional baselines under `bench/baselines/`, for example:

```bash
./build-bench/bench/lowl_audio_bench \
  --benchmark_repetitions=10 \
  --benchmark_report_aggregates_only=true \
  --benchmark_min_time=0.1s \
  --benchmark_out=bench/baselines/macos-arm64-release.json \
  --benchmark_out_format=json
```

Compare a new run against a saved baseline:

```bash
python3 bench/compare_baseline.py \
  bench/baselines/macos-arm64-release.json \
  bench/results/current.json \
  --warn-threshold 0.05 \
  --fail-threshold 0.10
```

## Notes

- Benchmarks are not part of the normal test suite.
- Run them on a quiet machine in Release mode.
- Save separate baselines per platform and architecture.
- The suite includes a short render baseline, a longer `AudioSpace` + `AudioVoice` path, and an `AudioStream` refill + render path.
- Prefer the `AudioSpace` and `AudioStream` scenarios for baseline tracking; they are closer to real playback workloads than the synthetic constant-source benchmark.
