#include "../include/lowl.h"

#include "lowl_audio_reader_wav.h"

#ifdef LOWL_DRIVER_DUMMY

#include "lowl_dummy_driver.h"

#endif

#ifdef LOWL_DRIVER_PORTAUDIO

#include "lowl_pa_driver.h"

#endif

std::vector<LowlDriver *> Lowl::drivers = std::vector<LowlDriver *>();


std::vector<LowlDriver *> Lowl::get_drivers(LowlError &error) {
    return drivers;
}

void Lowl::initialize(LowlError &error) {
#ifdef LOWL_DRIVER_DUMMY
    drivers.push_back(new LowlDummyDriver());
#endif
#ifdef LOWL_DRIVER_PORTAUDIO
    PaError pa_error = Pa_Initialize();
    if (pa_error != PaErrorCode::paNoError) {
        error.set_error(LowlError::Code::Error);
        return;
    }
    drivers.push_back(new LowlPaDriver());
#endif
}

void Lowl::terminate(LowlError &error) {
#ifdef LOWL_DRIVER_PORTAUDIO
    PaError pa_error = Pa_Terminate();
    if (pa_error != PaErrorCode::paNoError) {
        error.set_error(LowlError::Code::Error);
        return;
    }
#endif
}

std::unique_ptr<LowlAudioStream>
Lowl::create_stream(void *p_buffer, uint32_t p_length, LowlFileFormat format, LowlError &error) {
    std::unique_ptr<LowlAudioReader> reader = create_reader(format, error);
    if (error.has_error()) {
        return nullptr;
    }
    if (!reader) {
        error.set_error(LowlError::Code::Error);
        return nullptr;
    }
    std::unique_ptr<LowlAudioStream> stream = reader->read_ptr(p_buffer, p_length, error);
    if (error.has_error()) {
        return nullptr;
    }
    return stream;
}

std::unique_ptr<LowlAudioStream> Lowl::create_stream(const std::string &p_path, LowlError &error) {
    LowlFileFormat format = detect_format(p_path, error);
    if (error.has_error()) {
        return nullptr;
    }
    if (format == LowlFileFormat::UNKNOWN) {
        error.set_error(LowlError::Code::Error);
        return nullptr;
    }
    std::unique_ptr<LowlAudioReader> reader = create_reader(format, error);
    if (error.has_error()) {
        return nullptr;
    }
    if (!reader) {
        error.set_error(LowlError::Code::Error);
        return nullptr;
    }
    std::unique_ptr<LowlAudioStream> stream = reader->read_file(p_path, error);
    if (error.has_error()) {
        return nullptr;
    }
    if (!stream) {
        error.set_error(LowlError::Code::Error);
        return nullptr;
    }
    return stream;
}

std::unique_ptr<LowlAudioReader> Lowl::create_reader(LowlFileFormat format, LowlError &error) {
    std::unique_ptr<LowlAudioReader> reader = std::unique_ptr<LowlAudioReader>();
    switch (format) {
        case LowlFileFormat::UNKNOWN: {
            error.set_error(LowlError::Code::Error);
            break;
        }
        case LowlFileFormat::WAV: {
            reader = std::make_unique<LowlAudioReaderWav>();
            break;
        }
    }
    return reader;
}

LowlFileFormat Lowl::detect_format(const std::string &p_path, LowlError &error) {
    std::string::size_type idx;
    idx = p_path.rfind('.');
    if (idx != std::string::npos) {
        std::string extension = p_path.substr(idx + 1);
        std::transform(extension.begin(), extension.end(), extension.begin(), std::tolower);
        if (extension == "wav") {
            return LowlFileFormat::WAV;
        }
    }
    return LowlFileFormat::UNKNOWN;
}
