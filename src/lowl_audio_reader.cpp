#include "lowl_audio_reader.h"

#include "lowl_file.h"
#include "lowl_audio_reader_wav.h"
#include "lowl_audio_reader_mp3.h"
#include "lowl_audio_reader_flac.h"

#include <algorithm>

std::unique_ptr<Lowl::AudioData> Lowl::AudioReader::read_file(const std::string &p_path, Error &error) {
    LowlFile file = LowlFile();
    file.open(p_path, error);
    if (error.has_error()) {
        return nullptr;
    }
    size_t length = file.get_length();
    std::unique_ptr<uint8_t[]> buffer = file.read_buffer(length);
    std::unique_ptr<AudioData> audio_data = read(std::move(buffer), length, error);
    if (error.has_error()) {
        return nullptr;
    }
    if (!audio_data) {
        error.set_error(ErrorCode::Error);
        return nullptr;
    }
    return audio_data;
}

void Lowl::AudioReader::set_sample_converter(std::unique_ptr<SampleConverter> p_sample_converter) {
    sample_converter = std::move(p_sample_converter);
}

Lowl::AudioReader::AudioReader() {
    sample_converter = std::make_unique<SampleConverter>();
}

std::vector<Lowl::AudioFrame>
Lowl::AudioReader::read_frames(AudioFormat p_audio_format, SampleFormat p_sample_format, Channel p_channel,
                               const std::unique_ptr<uint8_t[]> &p_buffer, size_t p_size, Error &error) {

    size_t sample_size = get_sample_size(p_sample_format);
    size_t num_samples = p_size / sample_size;
    std::vector<float> samples = std::vector<float>();

    if (p_audio_format == AudioFormat::WAVE_FORMAT_PCM
        || p_audio_format == AudioFormat::WAVE_FORMAT_IEEE_FLOAT
        || p_audio_format == AudioFormat::MP3
        || p_audio_format == AudioFormat::FLAC
            ) {
        // formats can be just read based on sample format, no special handling required
        switch (p_sample_format) {
            case SampleFormat::INT_32: {
                if (p_size < sizeof(int32_t)) {
                    break;
                }
                int32_t *int32 = reinterpret_cast<int32_t *>(p_buffer.get());
                for (size_t current_sample = 0; current_sample < num_samples; current_sample++) {
                    int32_t sample_32 = int32[current_sample];
                    float sample = sample_converter->to_float(sample_32);
                    samples.push_back(sample);
                }
                break;
            }
            case SampleFormat::INT_16: {
                if (p_size < sizeof(int16_t)) {
                    break;
                }
                int16_t *int16 = reinterpret_cast<int16_t *>(p_buffer.get());
                for (size_t current_sample = 0; current_sample < num_samples; current_sample++) {
                    int16_t sample_16 = int16[current_sample];
                    float sample = sample_converter->to_float(sample_16);
                    samples.push_back(sample);
                }
                break;
            }
            case SampleFormat::FLOAT_32: {
                if (p_size < sizeof(float)) {
                    break;
                }
                float *sample_float = reinterpret_cast<float *>(p_buffer.get());
                for (size_t current_sample = 0; current_sample < num_samples; current_sample++) {
                    float sample = sample_float[current_sample];
                    samples.push_back(sample);
                }
                break;
            }
            default: {
                // error SampleFormat not supported for AudioFormat
                break;
            }
        }
    } else {
        // audio format not supported
    }

    std::vector<AudioFrame> frames = std::vector<AudioFrame>();

    if (samples.empty()) {
        // no data
        return frames;
    }

    switch (p_channel) {
        case Channel::Mono: {
            for (float sample : samples) {
                AudioFrame frame{};
                frame.left = sample;
                frame.right = sample;
                frames.push_back(frame);
            }
            break;
        }
        case Channel::Stereo: {
            size_t num_channels = get_channel_num(p_channel);
            size_t num_frames = num_samples / num_channels;
            for (size_t current_frame = 0; current_frame < num_frames; current_frame++) {
                AudioFrame frame{};
                size_t sample_index = current_frame * 2;
                frame.left = samples[sample_index];
                frame.right = samples[sample_index + 1];
                frames.push_back(frame);
            }
            break;
        }
        default: {
            // channels not supported
            break;
        }
    }

    return frames;
}

std::unique_ptr<Lowl::AudioReader> Lowl::AudioReader::create_reader(FileFormat format, Error &error) {
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
        case FileFormat::FLAC: {
            reader = std::make_unique<AudioReaderFlac>();
            break;
        }
    }
    return reader;
}

Lowl::FileFormat Lowl::AudioReader::detect_format(const std::string &p_path, Error &error) {
    std::string::size_type idx;
    idx = p_path.rfind('.');
    if (idx != std::string::npos) {
        std::string extension = p_path.substr(idx + 1);
        std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);
        if (extension == "wav") {
            return FileFormat::WAV;
        } else if (extension == "mp3") {
            return FileFormat::MP3;
        } else if (extension == "flac") {
            return FileFormat::FLAC;
        }
    }
    return FileFormat::UNKNOWN;
}

std::unique_ptr<Lowl::AudioData> Lowl::AudioReader::create_data(const std::string &p_path, Lowl::Error &error) {
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
    std::unique_ptr<AudioData> audio_data = reader->read_file(p_path, error);
    if (error.has_error()) {
        return nullptr;
    }
    if (!audio_data) {
        error.set_error(ErrorCode::Error);
        return nullptr;
    }
    return audio_data;
}

std::unique_ptr<Lowl::AudioData>
Lowl::AudioReader::create_data(std::unique_ptr<uint8_t[]> p_buffer, size_t p_size, Lowl::FileFormat p_format,
                               Lowl::Error &error) {
    std::unique_ptr<AudioReader> reader = create_reader(p_format, error);
    if (error.has_error()) {
        return nullptr;
    }
    if (!reader) {
        error.set_error(ErrorCode::Error);
        return nullptr;
    }
    std::unique_ptr<AudioData> audio_data = reader->read(std::move(p_buffer), p_size, error);
    if (error.has_error()) {
        return nullptr;
    }
    return audio_data;
}