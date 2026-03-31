# lowl_audio

[![CI](https://github.com/sebastian-heinz/lowl_audio/actions/workflows/main.yaml/badge.svg)](https://github.com/sebastian-heinz/lowl_audio/actions/workflows/main.yaml)
[![License](https://img.shields.io/github/license/sebastian-heinz/lowl_audio)](LICENSE)
[![C++17](https://img.shields.io/badge/C%2B%2B-17-00599C.svg)](https://isocpp.org/)
[![CMake](https://img.shields.io/badge/CMake-3.31%2B-064F8C.svg)](https://cmake.org/)

`lowl_audio` is a small C++ audio library for native playback, decoded file loading, and handle-based playback control.

It includes file readers, a mixer, streaming primitives, and `AudioSpace`, a higher-level API for loading assets and managing playback instances by handle.

## Features

- Native output backends for CoreAudio on macOS and WASAPI on Windows
- Dummy driver for tests and headless builds
- Decoders for WAV, MP3, OGG/Vorbis, FLAC, and Opus
- Float32 internal audio pipeline
- `AudioData`, `AudioVoice`, `AudioStream`, `AudioMixer`, and `AudioSpace`
- Per-playback controls for play, pause, resume, stop, seek, volume, and panning
- Bus routing in `AudioSpace` with `master_bus()`, `create_bus()`, and per-bus gain/panning

## Status

- Supported platforms: macOS, Windows
- Linux backend: not implemented yet
- Build requirements: CMake 3.31+, C++17
- License: MIT

## Build

```bash
git clone https://github.com/sebastian-heinz/lowl_audio.git
cd lowl_audio
git submodule update --init --recursive
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

If you want the test binary:

```bash
cmake --build build --target lowl_audio_test
./build/test/lowl_audio_test
```

Benchmarks are optional. Use a dedicated Release build for them:

Run these commands from the repository root.

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

## CMake Integration

```cmake
add_subdirectory(lowl_audio)
target_link_libraries(your_app PRIVATE lowl_audio_lib)
```

## Quick Start

```cpp
#include <lowl.h>

#include <memory>

int main() {
    Lowl::Error error;

    auto device = Lowl::Lib::get_default_device(error);
    if (error.has_error() || !device) {
        return 1;
    }

    const auto properties_list = device->get_properties_list();
    if (properties_list.empty()) {
        return 1;
    }
    const auto properties = properties_list.front();

    auto space = std::make_shared<Lowl::Audio::AudioSpace>(
        properties.sample_rate,
        properties.channel_layout
    );

    const auto asset = space->add_audio("music.wav", error);
    if (error.has_error() || !asset.is_valid()) {
        return 1;
    }

    const auto music_bus = space->create_bus(space->master_bus());
    if (!music_bus.is_valid()) {
        return 1;
    }

    space->set_volume(music_bus, 0.75f);

    const auto playback = space->create_playback(asset, music_bus);
    if (!playback.is_valid()) {
        return 1;
    }

    device->start(properties, space, error);
    if (error.has_error()) {
        return 1;
    }

    space->play(playback);

    // ...

    space->stop(playback);
    space->destroy_playback(playback);
    space->remove_audio(asset);
    device->stop(error);
    return error.has_error() ? 1 : 0;
}
```

For a fuller interactive example, see `demo/main.cpp`.

## Notes

- `AudioSpace` keeps assets and playbacks separate. `create_playback()` gives you one playback instance for one asset handle.
- `AudioSpace` exposes a master bus plus dynamically created child buses for grouping and independent control.
- `destroy_playback()` releases a playback slot when you are done with it.
- `destroy_bus()` retires an empty child bus; the master bus is permanent.
- `remove_audio()` retires an asset handle; existing playbacks keep their own shared reference to decoded audio data.
- Library initialization is one-shot. `terminate()` is process-shutdown only.

## Contributing

Issues and pull requests are welcome. Small, reviewable changes are easiest to work with.

## License

MIT. See [LICENSE](LICENSE).
