#include "../include/lowl.h"

#ifdef LOWL_DRIVER_DUMMY

#include "lowl_driver_dummy.h"

#endif

#ifdef LOWL_DRIVER_PORTAUDIO

#include "lowl_driver_pa.h"

#endif

#include <memory>

std::vector<std::shared_ptr<Lowl::Driver>> Lowl::Lib::drivers = std::vector<std::shared_ptr<Lowl::Driver>>();
std::atomic_flag Lowl::Lib::initialized = ATOMIC_FLAG_INIT;

std::vector<std::shared_ptr<Lowl::Driver>> Lowl::Lib::get_drivers(Error &error) {
    return drivers;
}

void Lowl::Lib::initialize(Error &error) {
    if (!initialized.test_and_set()) {
#ifdef LOWL_DRIVER_DUMMY
        drivers.push_back(std::make_shared<DummyDriver>());
#endif
#ifdef LOWL_DRIVER_PORTAUDIO
        PaError pa_error = Pa_Initialize();
        if (pa_error != PaErrorCode::paNoError) {
            error.set_error(Lowl::ErrorCode::Error);
            return;
        }
        drivers.push_back(std::make_shared<PaDriver>());
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

std::shared_ptr<Lowl::Device> Lowl::Lib::get_default_device(Lowl::Error &error) {
    // this might be a bit opinionated if we have multiple drivers.
    return std::shared_ptr<Device>();
}
