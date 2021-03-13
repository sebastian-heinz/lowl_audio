#include "../include/lowl.h"

#include "lowl_audio_reader_wav.h"
#include "lowl_audio_reader_mp3.h"

#ifdef LOWL_DRIVER_DUMMY

#include "lowl_driver_dummy.h"

#endif

#ifdef LOWL_DRIVER_PORTAUDIO

#include "lowl_driver_pa.h"

#endif

std::vector<Lowl::Driver *> Lowl::Lib::drivers = std::vector<Lowl::Driver *>();

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
Lowl::Lib::create_stream(std::unique_ptr<uint8_t[]> p_buffer, size_t p_size, Lowl::FileFormat p_format,
                         Lowl::Error &error) {
    std::unique_ptr<AudioReader> reader = create_reader(p_format, error);
    if (error.has_error()) {
        return nullptr;
    }
    if (!reader) {
        error.set_error(ErrorCode::Error);
        return nullptr;
    }
    std::unique_ptr<AudioStream> stream = reader->read(std::move(p_buffer), p_size, error);
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
        case FileFormat::MP3: {
            reader = std::make_unique<AudioReaderMp3>();
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
        } else if (extension == "mp3") {
            return FileFormat::MP3;
        }
    }
    return FileFormat::UNKNOWN;
}