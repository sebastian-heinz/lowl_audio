LowL Audio
===
Low Latency Audio - Audio Framework / Audio Engine

![example workflow](https://github.com/sebastian-heinz/lowl_audio/actions/workflows/main.yaml/badge.svg?branch=master)

## Setup
1) `git clone https://github.com/sebastian-heinz/lowl_audio.git`
2) `cd lowl_audio`
3) `git submodule update --init --recursive`

#### Flags
- LOWL_WIN - build for windows
- LOWL_UNIX - build for unix
- LOWL_DRIVER_DUMMY - enable dummy driver
- LOWL_DRIVER_PORTAUDIO - enable port audio driver

## Platforms
- Linux
- Windows
- macOS

## Features
- .wav / .mp3 / .flac parsing to AudioFrames
- audio playback
- sample rate conversion
- mixer that combines multiple input sources
- sample bit depth converter

---

## Usage
#### iterate drivers and devices
```c++
#include "lowl.h"

#include <iostream>

int main() 
{
    // initialize the library
    Lowl::Error error;
    Lowl::Lib::initialize(error);
    if (error.has_error()) {
        std::cout << "Err: Lowl::initialize\n";
        return -1;
    }

    // query a list of available drivers
    std::vector<std::shared_ptr<Lowl::Driver>> drivers = Lowl::Lib::get_drivers(error);
    if (error.has_error()) {
        std::cout << "Err: Lowl::get_drivers\n";
        return -1;
    }

    std::vector<std::shared_ptr<Lowl::Device>> all_devices = std::vector<std::shared_ptr<Lowl::Device>>();
    int current_device_index = 0;
    // iterate available drivers
    for (std::shared_ptr<Lowl::Driver> driver : drivers) {
        std::cout << "Driver: " + driver->get_name() + "\n";
        
        // driver need to be initialized before device can be queried
        driver->initialize(error);
        if (error.has_error()) {
            std::cout << "Err: driver->initialize (" + driver->get_name() + ")\n";
            error = Lowl::Error();
        }
        
        // iterate all device of a particular driver
        std::vector<std::shared_ptr<Lowl::Device>> devices = driver->get_devices();
        for (std::shared_ptr<Lowl::Device> device : devices) {
            std::cout << "Device[" + std::to_string(current_device_index++) + "]: " + device->get_name() + "\n";
            all_devices.push_back(device);
        }
    }
    
    // select a device
    int selected_index = 0;
    std::cout << "Select Device:\n";
    std::string user_input;
    std::getline(std::cin, user_input);
    selected_index = std::stoi(user_input);
    
    std::shared_ptr<Lowl::Device> device = all_devices[selected_index];
}
```
---
#### load audio file
```c++
#include "lowl.h"

#include <iostream>

int main() 
{
    std::shared_ptr<Lowl::AudioData> data = Lowl::Lib::create_data("/Users/name/Downloads/music.wav", error);
    if (error.has_error()) {
        std::cout << "Err:  Lowl::create_stream\n";
        return -1;
    }
}
```
---
#### audio playback
```c++
#include "lowl.h"

#include <iostream>
#include <thread>

int main() 
{
    std::shared_ptr<Lowl::Device> device = .... (refer to: iterate drivers and devices)
    std::shared_ptr<Lowl::AudioData> data = ... (refer to: load audio file)
    
    device->start(data, error);
    if (error.has_error()) {
        std::cout << "Err: device->start\n";
        return -1;
    }

    // wait till all frames have been played
    while (data->frames_remaining() > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        std::cout << "==PLAYING==\n";
        std::cout << "frames remaining: \n" + std::to_string(data->frames_remaining()) + "\n";
    }
}
```
---
#### audio mixer
```c++
#include "lowl.h"

#include <iostream>
#include <thread>

int main() 
{
    std::shared_ptr<Lowl::Device> device = ...... (refer to: iterate drivers and devices)
    std::shared_ptr<Lowl::AudioData> data_1 = ... (refer to: load audio file)
    std::shared_ptr<Lowl::AudioData> data_2 = ... (refer to: load audio file)
            
    std::shared_ptr<Lowl::AudioMixer> mixer = std::make_unique<Lowl::AudioMixer>(
            data_1->get_sample_rate(), data_1->get_channel()
    );
    
    // add data to the mixer
    // note: data can be added to the mixer at any point in time.
    // for example the device could be started first, then
    // data_1 can be mixed in after 1 second and data_2 after 40 seconds for example.
    mixer->mix_data(data_1);
    mixer->mix_data(data_2);

    // start the device
    device->start(data, error);
    if (error.has_error()) {
        std::cout << "Err: device->start\n";
        return -1;
    }

    // wait till all frames have been played
    while (data->frames_remaining() > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        std::cout << "==PLAYING==\n";
        std::cout << "frames remaining: \n" + std::to_string(data->frames_remaining()) + "\n";
    }
}
```
---
#### lowl space
```c++
#include "lowl.h"

#include <iostream>

int main() 
{
    // create a Lowl::AudioSpace
    // note: the Lowl::AudioSpace represents a collection of sounds, a use case might be a game as
    // one can load all sounds of a level into a space as part of the loading process.
    // and the playback is managed via an Id rather than requirement to manage references.
    std::shared_ptr<Lowl::AudioSpace> space = std::make_shared<Lowl::AudioSpace>();
    
    Lowl::Error error;
    
    // add audio files to it (.wav / .mp3 / .flac)
    // note: when adding a sound, it will return a Lowl::SpaceId, this id is used to play the corresponding sound.
    Lowl::SpaceId id = space->add_audio("/Users/railgun/Downloads/CantinaBand60.wav", error);
    
    // for this example we will not use the Lowl::SpaceId but let the user enter a Id to play.
    space->add_audio("/Users/railgun/Downloads/StarWars60.wav", error);

    // after all audio is registered, it needs to be loaded.
    // note: the load call will resample all audio files and bring them to the same sample rate and channel count.
    // based on the most frequent sample rate in the space.
    // if a specific sample rate should be used this can be set via space->set_sample_rate() or
    // space->set_channel()
    space->load();
    
    std::shared_ptr<Lowl::Device> device = ...... (refer to: iterate drivers and devices)
    // start the device        
    device->start(space->get_mixer(), error);
    if (error.has_error()) {
        std::cout << "Err: device->start\n";
        return -1;
    }

    // run until a invalid Lowl::SpaceId was selected
    while (true) {
        Lowl::SpaceId selected_id = Lowl::AudioSpace::InvalidSpaceId;
        std::cout << "Select Sound:\n";
        std::string user_input;
        std::getline(std::cin, user_input);
        try {
            selected_id = std::stoul(user_input);
        } catch (const std::exception &e) {
            continue;
        }
        if (selected_id <= Lowl::AudioSpace::InvalidSpaceId) {
            // id not available, stop
            std::cout << "Stop Selecting SpaceId\n";
            break;
        } else {
            // play selected id
            space->play(selected_id);
            // note: playing the same id before it has finished will cause the sound to abort and start from beginning again
            // a sound can also be cancelled anytime via space->stop().
        }
        std::cout << "frames remaining: \n" + std::to_string(space->get_mixer()->frames_remaining()) + "\n";
    }
}
```
---
#### configure logging
```c++
#include "lowl.h"

int main() 
{
    // set desired log level
    Lowl::Logger::set_log_level(Lowl::Logger::Level::Debug);
    // write logs to std::cout
    Lowl::Logger::register_std_out_log_receiver();
}
```
---
#### configure custom log receiver
```c++
#include "lowl.h"

void std_out_log_receiver(Logger::Level p_level, const char *p_message, void *p_user_data) {
    // handle logs
    std::cout << p_message;
}

void main()
{
    // calls std_out_log_receiver for handling logs
    register_log_receiver(&std_out_log_receiver, nullptr);
}
```

---

## Requirements
- all operations are performed over float32 audio frames
  - input files via the `AudioReader` are converted to float32 audio frames
  - `AudioFrame`s that have been generated through algorithms require to be in float32 format

## Info
- Pa+Win: if the sample rate of `AudioSource` that is passed to `LowlDevice` does not match the devices sample rate, it will not open the stream.

## Definitions
- Audio Sample = smallest audio unit, depends on bit depth
- Audio Frame = contains Audio Samples (2Channel/Stereo audio at 16bit depth contains two samples, each 2bytes. The Frame would be 4byte)

## System
![](./doc/system.jpg)
created with [draw.io](https://draw.io/)

---

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
- [doctest](https://github.com/onqtam/doctest)
  - License: [MIT](https://github.com/onqtam/doctest/blob/master/LICENSE.txt)
  - C++ testing framework
  
---

## License
All third party libraries come with own licenses and terms.
You can find the respective license in the 3rd parties directory.
For convenience the specific license for each project that is used can be found in the above link collection.
However all 3rd party dependencies have been chosen to be as friendly as possible and in all cases retaining the
license text / copyright notice when distributing them in source or binary form will have you covered.

This project itself is licensed under the MIT license, that includes the .h and .cpp files, except for the content
in the `third_party`-directory.

---

## Links
a list of related information to audio programming

- [real time audio programming 101](http://www.rossbencina.com/code/real-time-audio-programming-101-time-waits-for-nothing)
- [int float int its jungle out there](http://blog.bjornroche.com/2009/12/int-float-int-its-jungle-out-there.html)
- [linearity and dynamic range in int](http://blog.bjornroche.com/2009/12/linearity-and-dynamic-range-in-int.html)
- [Audio recording bitdepth](https://lists.apple.com/archives/coreaudio-api/2009/Dec/msg00046.html)
- [CppCon 2015: Timur Doumler “C++ in the Audio Industry”](https://www.youtube.com/watch?v=boPEO2auJj4)
---

## TODO
- .ogg format
- c-api wrapper / - LOWL_LIBRARY - build as library flag
- potentially isolate 3rd party in wrapper.
  - ex. LowlQueue -> wraps queue, etc. and provide a way to provide different implementation
  - similar to driver / audio reader abstraction
- speed up code, but prefer simplicity
- feedback on API surface / user friendly / self explaining
- finding / fixing bugs - make it more robust
