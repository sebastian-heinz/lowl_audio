#include "lowl.h"

#ifdef LOWL_DRIVER_DUMMY
#include "audio/backend/dummy/lowl_audio_dummy_driver.h"
#endif

#ifdef LOWL_DRIVER_CORE_AUDIO
#include "audio/backend/coreaudio/lowl_audio_core_audio_driver.h"
#endif

#ifdef LOWL_DRIVER_WASAPI
#include "audio/backend/wasapi/lowl_audio_wasapi_com.h"
#include "audio/backend/wasapi/lowl_audio_wasapi_driver.h"
#endif

std::vector<std::shared_ptr<Lowl::Audio::AudioDriver>> Lowl::Lib::drivers =
    std::vector<std::shared_ptr<Audio::AudioDriver>>();
std::once_flag Lowl::Lib::initialized;
std::atomic<bool> Lowl::Lib::terminated{false};
Lowl::Error Lowl::Lib::initialization_error;

std::vector<std::shared_ptr<Lowl::Audio::AudioDriver>> Lowl::Lib::get_drivers(Error &error) {
    initialize(error);
    if (error.has_error()) {
        return {};
    }
    return drivers;
}

void Lowl::Lib::initialize(Lowl::Error &error) {
    if (terminated.load(std::memory_order_acquire)) {
        LOWL_LOG_ERROR("Lowl::Lib::initialize called after Lib::terminate; terminate() is process-shutdown only.");
        error.set_error(ErrorCode::Error);
        return;
    }
    std::call_once(initialized, []() {
        initialization_error.clear();
#ifdef LOWL_DRIVER_DUMMY
        drivers.push_back(std::make_shared<Lowl::Audio::AudioDriverDummy>());
#endif
#ifdef LOWL_DRIVER_CORE_AUDIO
        drivers.push_back(std::make_shared<Lowl::Audio::CoreAudioDriver>());
#endif
#ifdef LOWL_DRIVER_WASAPI
        Error wasapi_err;
        Lowl::Audio::WasapiCom::wasapi_com->initialize(wasapi_err);
        if (wasapi_err.ok()) {
            drivers.push_back(std::make_shared<Lowl::Audio::WasapiDriver>());
        } else {
            initialization_error = wasapi_err;
        }
#endif
    });
    error = initialization_error;
}

void Lowl::Lib::terminate(Error &error) {
    (void)error;
    terminated.store(true, std::memory_order_release);
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

std::unique_ptr<Lowl::Audio::AudioData> Lowl::Lib::create_data(std::unique_ptr<uint8_t[]> p_buffer,
                                                               size_t p_size,
                                                               Lowl::FileFormat p_format,
                                                               Lowl::Error &error) {
    return Lowl::Audio::AudioReader::create_data(std::move(p_buffer), p_size, p_format, error);
}

std::unique_ptr<Lowl::Audio::AudioData> Lowl::Lib::create_data(const std::string &p_path, Lowl::Error &error) {
    return Lowl::Audio::AudioReader::create_data(p_path, error);
}

std::shared_ptr<Lowl::Audio::AudioDevice> Lowl::Lib::get_default_device(Lowl::Error &error) {
    initialize(error);
    if (error.has_error()) {
        return {};
    }
    for (auto it = drivers.rbegin(); it != drivers.rend(); ++it) {
        std::shared_ptr<Lowl::Audio::AudioDevice> default_device = (*it)->get_default_device();
        if (default_device) {
            return default_device;
        }
    }
    return std::shared_ptr<Lowl::Audio::AudioDevice>();
}
