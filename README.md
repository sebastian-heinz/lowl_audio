LowL Audio
===
Low Latency Audio - Audio Framework / Audio Engine

# mostly working but still WIP, looking for help
- speed up code, but prefer simplicity
- feedback on API surface / user friendly / self explaining
- testing under different systems (osx / unix / win)
- finding / fixing bugs - make it more robust

## Goal
This project aims to use exiting 3rd party libraries and combine them into an audio framework.

- audio format parsing
- sample conversation
- playback
- unix/osx/win support
- easy to reason about code
- only utilize libraries that are compatible/comparable with MIT license
- modular

## Features
- .wav / .mp3 / .flac parsing to AudioFrames
- audio playback
- sample rate conversion
- mixer that combines multiple input sources
- sample bit depth converter

## Requirements
- all operations are performed over float32 audio frames
  - input files via the `AudioReader` are converted to float32 audio frames
  - `AudioFrame`s that have been generated through algorithms require to be in float32 format

## Classes
brief overview of the included classes

### AudioStream 
- a endless stream of audio data, AudioFrames can be pushed into it and read from. once a frame is read it is gone from the stream.

### AudioData 
- a finite set of audio frames, when the end is reached it will signal its end and next read call will return data from the start. (for effects / reoccuring sounds etc)

### AudioMixer 
- accepts AudioStream and AudioData, combines them to a single AudioStream
- has a mixing thread that constantly mixes frames, `start_mix()` and `stop_mix()` control this
- it is thread safe to add AudioData or AudioStreams to the mixer via `mix_data()` and `mix_stream()` method
- uses a lock free queue to pass events to the mixer, for adding AudioData or AudioStreams
- `mix_all()` - function will mix all frames until every input is exhausted. (dont use while mixing thread is active)
- `mix_next_frame()` will mix a single frame from all available inputs, returns false if no frame has been mixed. (dont use while mixing thread is active)

### ReSampler
- re samples AudioFrames from one SampleRate to another one.
- call `write(const AudioFrame &p_audio_frame, size_t p_required_frames)`, repeatedly, it will return true once the `required_frames` have been produced.
- initially the `ReSampler` has to be feed more frames to produce output.

### AudioDevice 
- represents a device like headphones or speaker

### AudioDriver 
- provides a list of devices for playback

### AudioReader
- wav/flac/mp3
- provides capabilities to read audio files and return a AudioStream

### AudioFrame 
- represents a single frame of audio data


## Setup
1) `git clone https://github.com/sebastian-heinz/lowl_audio.git`
2) `cd lowl_audio`
3) `git submodule update --init --recursive`

## 3rd Party
- [Port Audio](https://github.com/PortAudio/portaudio) 
  - License: [MIT](https://github.com/PortAudio/portaudio/blob/master/LICENSE.txt)
  - portable audio I/O library designed for cross-platform support of audio.
- [readerwriterqueue](https://github.com/cameron314/readerwriterqueue) & [concurrentqueue](https://github.com/cameron314/concurrentqueue)
  - License: [simplified BSD](https://github.com/cameron314/readerwriterqueue/blob/master/LICENSE.md)
  - [Blog Post](https://moodycamel.com/blog/2013/a-fast-lock-free-queue-for-c++.htm) describing the queue is designed for audio sample transfer
  - industrial-strength lock-free queue for C++
- [dr_libs](https://github.com/mackron/dr_libs)
  - License: [Choice of public domain or MIT-0](https://github.com/mackron/dr_libs/blob/46f149034a9f27e873d2c4c6e6a34ae4823a2d8d/dr_wav.h#L6363)
  - audio decoding libraries
- [r8brain-free-src](https://github.com/avaneev/r8brain-free-src)
  - License: [MIT](https://github.com/avaneev/r8brain-free-src/blob/master/LICENSE)
  - high-quality professional audio sample rate converter

## License
All third party libraries come with own licenses and terms.
You can find the respective license in the 3rd parties directory.
For convenience the specific license for each project that is used can be found in the above link collection.
However all 3rd party dependencies have been chosen to be as friendly as possible and in all cases retaining the
license text / copyright notice when distributing them in source or binary form will have you covered.

This project itself is licensed under the MIT license, that includes the .h and .cpp files, except for the content
in the `third_party`-directory.

## Guidlines

### Header include order:
from local to global, each subsection in alphabetical order, i.e.:
1) h file corresponding to this cpp file (if applicable)
2) headers from the same component
3) headers from other components
4) system headers

## System
![](./doc/system.jpg)
created with [draw.io](https://draw.io/)

## Definitions
- Audio Sample = smallest audio unit, depends on bit depth
- Audio Frame = contains Audio Samples (2Channel/Stereo audio at 16bit depth contains two samples, each 2bytes. The Frame would be 4byte)

## Links
a list of related information to audio programming

- [real time audio programming 101](http://www.rossbencina.com/code/real-time-audio-programming-101-time-waits-for-nothing)
- [int float int its jungle out there](http://blog.bjornroche.com/2009/12/int-float-int-its-jungle-out-there.html)
- [linearity and dynamic range in int](http://blog.bjornroche.com/2009/12/linearity-and-dynamic-range-in-int.html)
- [Audio recording bitdepth](https://lists.apple.com/archives/coreaudio-api/2009/Dec/msg00046.html)

## Flags
- LOWL_LIBRARY - build as library
- LOWL_WIN - build for windows
- LOWL_UNIX - build for unix  
- LOWL_DRIVER_DUMMY - enable dummy driver
- LOWL_DRIVER_PORTAUDIO - enable port audio driver

## TODO
- linux testing
- .ogg format
- c-api wrapper
- potentially isolate 3rd party in wrapper.
  - ex. LowlQueue -> wraps queue, etc. and provide a way to provide different implementation
  - similar to driver / audio reader abstraction

## Ideas
- workspace class
  - add sound files from start (they will be resampled and converted)
  - specify memory size, if not enough store tmp resampled data to disk
  - single point for configuration and playback