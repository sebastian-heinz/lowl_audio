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
- Planar `Sample` pipeline: float32 by default, or double with `LOWL_TYPE_SAMPLE_64`
- `AudioData`, `AudioVoice`, `AudioStream`, `AudioMixer`, and `AudioSpace`
- Per-playback controls for play, pause, resume, stop, seek, volume, and panning
- Explicit nested-mixer composition, with optional owning `AudioGraph` topology management

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

Run the tests through CTest:

```bash
ctest --test-dir build -R "^lowl_audio_test$" --output-on-failure
```

`Lowl::Sample` is `float` by default. Select the double-precision public ABI at
configure time:

```bash
cmake -S . -B build-double \
  -DCMAKE_BUILD_TYPE=Release \
  -DLOWL_TYPE_SAMPLE_64=ON
cmake --build build-double
ctest --test-dir build-double -R "^lowl_audio_test$" --output-on-failure
```

All targets that exchange `Lowl::Sample` values must use the same setting.

Benchmarks are optional. Use a dedicated Release build for them:

Run these commands from the repository root.

```bash
cmake -S . -B build-bench -DCMAKE_BUILD_TYPE=Release -DLOWL_BUILD_BENCHMARKS=ON
cmake --build build-bench --target lowl_audio_bench
```

## Run Benchmarks

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

    auto space =
        std::make_shared<Lowl::Audio::AudioSpace>(properties.audio_format);

    const auto asset = space->add_audio("music.wav", error);
    if (error.has_error() || !asset.is_valid()) {
        return 1;
    }

    const auto playback = space->create_playback(asset, error);
    if (error.has_error() || !playback.is_valid()) {
        return 1;
    }

    space->set_volume(playback, 0.75f, error);
    if (error.has_error()) {
        return 1;
    }

    device->start(properties, space, error);
    if (error.has_error()) {
        return 1;
    }

    space->play(playback, error);
    if (error.has_error()) {
        return 1;
    }

    // ...

    space->stop(playback, error);
    if (error.has_error()) {
        return 1;
    }

    // Request retirement while the device is still pulling the Space.
    space->destroy_playback(playback, error);
    if (error.has_error()) {
        return 1;
    }

    space->remove_audio(asset, error);
    if (error.has_error()) {
        return 1;
    }

    device->stop(error);
    return error.has_error() ? 1 : 0;
}
```

For a fuller interactive example, see `demo/main.cpp`.

## Manual Mixer Composition

`AudioMixer` connections are non-owning. Keep every source alive until the
matching terminal completion is collected:

```cpp
Lowl::Error error;
const Lowl::Audio::AudioFormat format = properties.audio_format;

auto root = std::make_shared<Lowl::Audio::AudioMixer>(format);
auto submix = std::make_unique<Lowl::Audio::AudioMixer>(format);
auto stream = std::make_unique<Lowl::Audio::AudioStream>(format, 4096);

const auto submix_connection = root->connect(*submix, error);
if (error.has_error()) {
    return 1;
}
const auto stream_connection = submix->connect(*stream, error);
if (error.has_error()) {
    return 1;
}

device->start(properties, root, error);
if (error.has_error()) {
    return 1;
}

// Later, from the owning control thread:
submix->disconnect(stream_connection, error);
if (error.has_error()) {
    return 1;
}

bool submix_retiring = false;
auto update_composition = [&]() {
    Lowl::Audio::AudioMixerCompletion completion;

    // First wait for the nested connection before releasing the stream.
    if (stream && submix->try_collect_completion(completion) &&
        completion.handle == stream_connection) {
        stream.reset();
        root->disconnect(submix_connection, error);
        submix_retiring = !error.has_error();
    }

    // Then wait for the parent connection before releasing the submix.
    if (submix_retiring && root->try_collect_completion(completion) &&
        completion.handle == submix_connection) {
        submix.reset();
        submix_retiring = false;
    }
};

// Call update_composition() from the regular control/update loop while
// rendering continues; do not spin-wait on the render thread.
```

Each mixer has one logical controller and one completion consumer. A paused
mixer still processes lifecycle work, but it must continue to be pulled.

## Owning AudioGraph Composition

`AudioGraph` owns its nodes and validates topology. This example routes two
independent Spaces and one live stream through a nested mixer:

```cpp
Lowl::Error error;
const Lowl::Audio::AudioFormat format = properties.audio_format;
auto graph = std::make_shared<Lowl::Audio::AudioGraph>(format);

const auto submix =
    graph->create<Lowl::Audio::AudioMixer>(error, format);
if (error.has_error()) {
    return 1;
}
const auto music_space =
    graph->create<Lowl::Audio::AudioSpace>(error, format);
if (error.has_error()) {
    return 1;
}
const auto effects_space =
    graph->create<Lowl::Audio::AudioSpace>(error, format);
if (error.has_error()) {
    return 1;
}
const auto voice_stream =
    graph->create<Lowl::Audio::AudioStream>(error, format, 4096);
if (error.has_error()) {
    return 1;
}

graph->connect(graph->root(), submix, error);
if (error.has_error()) {
    return 1;
}
graph->connect(submix, music_space, error);
if (error.has_error()) {
    return 1;
}
graph->connect(submix, effects_space, error);
if (error.has_error()) {
    return 1;
}
graph->connect(submix, voice_stream, error);
if (error.has_error()) {
    return 1;
}

auto *music = graph->get<Lowl::Audio::AudioSpace>(music_space, error);
auto *stream = graph->get<Lowl::Audio::AudioStream>(voice_stream, error);
if (error.has_error() || music == nullptr || stream == nullptr) {
    return 1;
}

device->start(properties, graph, error);
if (error.has_error()) {
    return 1;
}

// The producer can now call stream->write_interleaved(...).
// Call graph->update() from the control loop to finalize asynchronous
// disconnect/destroy acknowledgements while the device keeps rendering.
```

Graph-owned mixers must be connected only through `AudioGraph`; calling their
low-level `AudioMixer::connect()` API bypasses graph bookkeeping.

## Notes

- `AudioSpace` keeps assets and playbacks separate. `create_playback()` gives you one playback instance for one asset handle.
- `AudioSpace` is one source with one private mixer. Route several Spaces through ordinary nested `AudioMixer` nodes when you need groups.
- `destroy_playback()` starts retirement. If the playback was connected, its slot is recycled only after the Space is pulled and later collects the mixer acknowledgement.
- `remove_audio()` retires an asset handle; existing playbacks keep their own shared reference to decoded audio data.
- Control operations report failure through the explicit `Lowl::Error &` argument and return early; they do not terminate the process.
- Library initialization is one-shot. `terminate()` is process-shutdown only.

## Contributing

Issues and pull requests are welcome. Small, reviewable changes are easiest to work with.

## License

MIT. See [LICENSE](LICENSE).
