#include "../include/lowl.h"

#include "lowl_audio_reader_wav.h"

#ifdef LOWL_DRIVER_DUMMY

#include "driver/dummy/lowl_dummy_driver.h"

#endif

#ifdef LOWL_DRIVER_PORTAUDIO

#include "driver/portaudio/lowl_pa_driver.h"

#endif

namespace {
    std::vector<Lowl::Driver *> drivers = std::vector<Lowl::Driver *>();
}

std::vector<Lowl::Driver *> Lowl::Lib::get_drivers(Error &error) {
    return drivers;
}

void Lowl::Lib::initialize(Error &error) {
#ifdef LOWL_DRIVER_DUMMY
    drivers.push_back(new DummyDriver());
#endif
#ifdef LOWL_DRIVER_PORTAUDIO
    PaError pa_error = Pa_Initialize();
    if (pa_error != PaErrorCode::paNoError) {
        error.set_error(ErrorCode::Error);
        return;
    }
    drivers.push_back(new PaDriver());
#endif
}

void Lowl::Lib::terminate(Error &error) {
#ifdef LOWL_DRIVER_PORTAUDIO
    PaError pa_error = Pa_Terminate();
    if (pa_error != PaErrorCode::paNoError) {
        error.set_error(ErrorCode::Error);
        return;
    }
#endif
}

std::unique_ptr<Lowl::AudioStream>
Lowl::Lib::create_stream(void *p_buffer, uint32_t p_length, FileFormat format, Error &error) {
    std::unique_ptr<AudioReader> reader = create_reader(format, error);
    if (error.has_error()) {
        return nullptr;
    }
    if (!reader) {
        error.set_error(ErrorCode::Error);
        return nullptr;
    }
    std::unique_ptr<AudioStream> stream = reader->read_ptr(p_buffer, p_length, error);
    if (error.has_error()) {
        return nullptr;
    }
    return stream;
}

std::unique_ptr<Lowl::AudioStream> Lowl::Lib::create_stream(const std::string &p_path, Error &error) {
    FileFormat format = detect_format(p_path, error);
    if (error.has_error()) {
        return nullptr;
    }
    if (format == FileFormat::UNKNOWN) {
        error.set_error(ErrorCode::Error);
        return nullptr;
    }
    std::unique_ptr<AudioReader> reader = create_reader(format, error);
    if (error.has_error()) {
        return nullptr;
    }
    if (!reader) {
        error.set_error(ErrorCode::Error);
        return nullptr;
    }
    std::unique_ptr<AudioStream> stream = reader->read_file(p_path, error);
    if (error.has_error()) {
        return nullptr;
    }
    if (!stream) {
        error.set_error(ErrorCode::Error);
        return nullptr;
    }
    return stream;
}

std::unique_ptr<Lowl::AudioReader> Lowl::Lib::create_reader(FileFormat format, Error &error) {
    std::unique_ptr<AudioReader> reader = std::unique_ptr<AudioReader>();
    switch (format) {
        case FileFormat::UNKNOWN: {
            error.set_error(ErrorCode::Error);
            break;
        }
        case FileFormat::WAV: {
            reader = std::make_unique<AudioReaderWav>();
            break;
        }
    }
    return reader;
}

Lowl::FileFormat Lowl::Lib::detect_format(const std::string &p_path, Error &error) {
    std::string::size_type idx;
    idx = p_path.rfind('.');
    if (idx != std::string::npos) {
        std::string extension = p_path.substr(idx + 1);
        std::transform(extension.begin(), extension.end(), extension.begin(), std::tolower);
        if (extension == "wav") {
            return FileFormat::WAV;
        }
    }
    return FileFormat::UNKNOWN;
}