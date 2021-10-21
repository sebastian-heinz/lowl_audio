#include "../include/lowl.h"

#ifdef LOWL_DRIVER_DUMMY
#include "lowl_audio_driver_dummy.h"
#endif
#ifdef LOWL_DRIVER_PORTAUDIO
#include "lowl_audio_driver_pa.h"
#endif
#ifdef LOWL_DRIVER_CORE_AUDIO
#include "lowl_audio_driver_core_audio.h"
#endif

#include <memory>

std::vector<std::shared_ptr<Lowl::AudioDriver>> Lowl::Lib::drivers = std::vector<std::shared_ptr<Lowl::AudioDriver>>();
std::atomic_flag Lowl::Lib::initialized = ATOMIC_FLAG_INIT;

std::vector<std::shared_ptr<Lowl::AudioDriver>> Lowl::Lib::get_drivers(Error &error) {
    return drivers;
}

void Lowl::Lib::initialize(Error &error) {
    if (!initialized.test_and_set()) {
#ifdef LOWL_DRIVER_DUMMY
        drivers.push_back(std::make_shared<AudioDriverDummy>());
#endif
#ifdef LOWL_DRIVER_PORTAUDIO
        PaError pa_error = Pa_Initialize();
        if (pa_error != PaErrorCode::paNoError) {
            error.set_error(Lowl::ErrorCode::Error);
            return;
        }
        drivers.push_back(std::make_shared<AudioDriverPa>());
#endif
#ifdef LOWL_DRIVER_CORE_AUDIO
        drivers.push_back(std::make_shared<AudioDriverCoreAudio>());
#endif
    }
}

void Lowl::Lib::terminate(Error &error) {
#ifdef LOWL_DRIVER_PORTAUDIO
    PaError pa_error = Pa_Terminate();
    if (pa_error != PaErrorCode::paNoError) {
        error.set_error(Lowl::ErrorCode::Error);
        return;
    }
#endif
}

std::unique_ptr<Lowl::AudioReader> Lowl::Lib::create_reader(Lowl::FileFormat p_format, Lowl::Error &error) {
    return Lowl::AudioReader::create_reader(p_format, error);
}

Lowl::FileFormat Lowl::Lib::detect_format(const std::string &p_path, Lowl::Error &error) {
    return Lowl::AudioReader::detect_format(p_path, error);
}

std::unique_ptr<Lowl::AudioData>
Lowl::Lib::create_data(std::unique_ptr<uint8_t[]> p_buffer, size_t p_size, FileFormat p_format, Lowl::Error &error) {
    return Lowl::AudioReader::create_data(std::move(p_buffer), p_size, p_format, error);
}

std::unique_ptr<Lowl::AudioData> Lowl::Lib::create_data(const std::string &p_path, Lowl::Error &error) {
    return Lowl::AudioReader::create_data(p_path, error);
}

std::shared_ptr<Lowl::AudioDevice> Lowl::Lib::get_default_device(Lowl::Error &error) {
    // This might be a bit opinionated if we have multiple drivers.
    // Iterates the drivers in reverse order, prioritizing the last added driver.
    // In the future it might be possible that a user can push a driver in the list
    // this will cause the last added driver to be checked first.
    for (auto it = drivers.rbegin(); it != drivers.rend(); ++it) {
        std::shared_ptr<AudioDevice> default_device = (*it)->get_default_device();
        if (default_device) {
            return default_device;
        }
    }
    return std::shared_ptr<AudioDevice>();;
}
