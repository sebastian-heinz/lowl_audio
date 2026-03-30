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

Benchmarks are optional:

```bash
cmake -S . -B build -DLOWL_BUILD_BENCHMARKS=ON
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

    const auto playback = space->create_playback(asset);
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

- `AudioSpace` keeps assets and playbacks separate. `create_playback()` gives you a playback handle for one asset instance.
- `destroy_playback()` releases a playback slot when you are done with it.
- `remove_audio()` retires an asset handle; existing playbacks keep their own shared reference to decoded audio data.
- Library initialization is one-shot. `terminate()` is process-shutdown only.

## Contributing

Issues and pull requests are welcome. Small, reviewable changes are easiest to work with.

## License

MIT. See [LICENSE](LICENSE).
