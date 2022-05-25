#include "lowl.h"

#ifdef LOWL_DRIVER_DUMMY

#include "audio/dummy/lowl_audio_dummy_driver.h"

#endif
#ifdef LOWL_DRIVER_PORTAUDIO

#include "audio/portaudio/lowl_audio_pa_driver.h"

#endif
#ifdef LOWL_DRIVER_CORE_AUDIO
#include "audio/coreaudio/lowl_audio_core_audio_driver.h"
#endif
#ifdef LOWL_DRIVER_WASAPI

#include "audio/wasapi/lowl_audio_wasapi_driver.h"
#include "audio/wasapi/lowl_audio_wasapi_com.h"

#endif

#include <memory>

std::vector<std::shared_ptr<Lowl::Audio::AudioDriver>> Lowl::Lib::drivers = std::vector<std::shared_ptr<Audio::AudioDriver>>();
std::atomic_flag Lowl::Lib::initialized = ATOMIC_FLAG_INIT;

std::vector<std::shared_ptr<Lowl::Audio::AudioDriver>> Lowl::Lib::get_drivers(Error &error) {
    return drivers;
}

void Lowl::Lib::initialize(Lowl::Error &error) {
    if (!initialized.test_and_set()) {
#ifdef LOWL_DRIVER_DUMMY
        drivers.push_back(std::make_shared<Lowl::Audio::AudioDriverDummy>());
#endif
#ifdef LOWL_DRIVER_PORTAUDIO
        PaError pa_error = Pa_Initialize();
        if (pa_error == PaErrorCode::paNoError) {
            drivers.push_back(std::make_shared<Lowl::Audio::AudioDriverPa>());
        } else {
            LOWL_LOG_ERROR_F("PortAudio failed Pa_Initialize (PaError:%d)", pa_error);
        }
#endif
#ifdef LOWL_DRIVER_CORE_AUDIO
        drivers.push_back(std::make_shared<Lowl::Audio::CoreAudioDriver>());
#endif
#ifdef LOWL_DRIVER_WASAPI
        Error wasapi_err;
        Lowl::Audio::WasapiCom::wasapi_com->initialize(wasapi_err);
        if (wasapi_err.ok()) {
            drivers.push_back(std::make_shared<Lowl::Audio::WasapiDriver>());
        }
#endif
    }
}

void Lowl::Lib::terminate(Error &error) {
#ifdef LOWL_DRIVER_PORTAUDIO
    PaError pa_error = Pa_Terminate();
    if (pa_error != PaErrorCode::paNoError) {
        LOWL_LOG_ERROR_F("PortAudio failed Pa_Terminate (PaError:%d)", pa_error);
        return;
    }
#endif
#ifdef LOWL_DRIVER_WASAPI
    Lowl::Audio::WasapiCom::wasapi_com->terminate();
#endif
}

std::unique_ptr<Lowl::Audio::AudioReader> Lowl::Lib::create_reader(Lowl::FileFormat p_format, Lowl::Error &error) {
    return Lowl::Audio::AudioReader::create_reader(p_format, error);
}

Lowl::FileFormat Lowl::Lib::detect_format(const std::string &p_path, Lowl::Error &error) {
    return Lowl::Audio::AudioReader::detect_format(p_path, error);
}

std::unique_ptr<Lowl::Audio::AudioData>
Lowl::Lib::create_data(std::unique_ptr<uint8_t[]> p_buffer, size_t p_size, Lowl::FileFormat p_format,
                       Lowl::Error &error) {
    return Lowl::Audio::AudioReader::create_data(std::move(p_buffer), p_size, p_format, error);
}

std::unique_ptr<Lowl::Audio::AudioData> Lowl::Lib::create_data(const std::string &p_path, Lowl::Error &error) {
    return Lowl::Audio::AudioReader::create_data(p_path, error);
}

std::shared_ptr<Lowl::Audio::AudioDevice> Lowl::Lib::get_default_device(Lowl::Error &error) {
    // This might be a bit opinionated if we have multiple drivers.
    // Iterates the drivers in reverse order, prioritizing the last added driver.
    // In the future it might be possible that a user can push a driver in the list
    // this will cause the last added driver to be checked first.
    for (auto it = drivers.rbegin(); it != drivers.rend(); ++it) {
        std::shared_ptr<Lowl::Audio::AudioDevice> default_device = (*it)->get_default_device();
        if (default_device) {
            return default_device;
        }
    }
    return std::shared_ptr<Lowl::Audio::AudioDevice>();;
}
