LowL Audio
===
Low Latency Audio - aims to provide audio playback - work in progress

This readme is a draft and subject to change, it represents my current state of thinking, notes and ideas.


## Disclaimer
- no audio expert
- just learning c++

## Goal
Providing audio output is quite difficult, one needs to parse sound files, 
process the samples so they can be consumed by the driver and lastly 
implement the audio driver for each platform.

All of these tasks are somewhat solved but I still find it challenging to
put it all together, and in the end a lot of 3rd party libraries impose licenses,
which are not always easy to apply.

The aim of this project is to be the glue between moving parts and provide:
- common audio format parsing
- sample conversation
- playback
- unix/osx/win support
- easy to understand/reason about code
- MIT licensed and only utilize libraries that are compatible with MIT license

As a user of this library I want to be able to play back audio samples, be it generated or from audio files.

Update:
- this library should focus on being the "glue" by utilizing open source projects
- it should be designed in a modular way
  - custom audio frame extraction support (dr_wav.. etc are just plug and play modules)
  - custom driver 
  - etc

Update:
- perhaps rename library to AudioBox..BoomBox etc
- utilize builder class and require user to select components (so that all parts are interchangable and one can have own / custom BOX)
  - driver
  - reader
  - converter
- provide sane default setting 


## Setup
1) `git clone https://github.com/sebastian-heinz/lowl_audio.git`
2) `cd lowl_audio`
3) `git submodule update --init --recursive`

## 3rd Party
- [Port Audio](https://github.com/PortAudio/portaudio) 
  - [MIT](https://github.com/PortAudio/portaudio/blob/master/LICENSE.txt)
  - provides playback devices
- [readerwriterqueue](https://github.com/cameron314/readerwriterqueue) 
  - [simplified BSD](https://github.com/cameron314/readerwriterqueue/blob/master/LICENSE.md)
  - [Blog Post](https://moodycamel.com/blog/2013/a-fast-lock-free-queue-for-c++.htm) describing the queue is designed for audio sample transfer
  - provides pushing frames to the driver
- [dr_libs](https://github.com/mackron/dr_libs)
  - [Choice of public domain or MIT-0](https://github.com/mackron/dr_libs/blob/46f149034a9f27e873d2c4c6e6a34ae4823a2d8d/dr_wav.h#L6363)
  - provide wav / mp3 / flac parsing

All third party libraries should be compatible with the MIT license,
so that they can be included.
If there is any issue including one of these projects under the MIT license,
please let me know by opening an issue and I will remove it / perform required changes.

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

## Supported Formats
any binary blob provided in a supported `data formats` can be used and
all combinations of `file formats` and `data formats` are supported.

When reading data the library will extract the audio samples and convert them to
sample format of 32bit float. This process will only happen once on reading, as 
every internal function is designed to operate on 32bit float samples.

### file formats
- .wav

### data formats
| sample format | channel |
|:--------------|:--------| 
|float 32bit    | mono    |
|float 32bit    | stereo  |
|int 16bit      | mono    |
|int 16bit      | stereo  |

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
LOWL_LIBRARY  
LOWL_WIN  
LOWL_UNIX  
LOWL_DRIVER_DUMMY  
LOWL_DRIVER_PORTAUDIO

## TODO
- file format parsing (mp3 / ogg)
- run under win/unix
- c-api wrapper


