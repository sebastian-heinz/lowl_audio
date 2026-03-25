#include "lowl_audio_reader.h"

#include <algorithm>
#include <memory>

#include "audio/reader/lowl_audio_reader_flac.h"
#include "audio/reader/lowl_audio_reader_mp3.h"
#include "audio/reader/lowl_audio_reader_ogg.h"
#include "audio/reader/lowl_audio_reader_opus.h"
#include "audio/reader/lowl_audio_reader_wav.h"
#include "lowl_file.h"

std::unique_ptr<Lowl::Audio::AudioData> Lowl::Audio::AudioReader::read_file(const std::string &p_path,
                                                                            Lowl::Error &error) {
    std::unique_ptr<Lowl::File> file = std::make_unique<Lowl::File>();
    file->open(p_path, error);
    if (error.has_error()) {
        return nullptr;
    }
    size_t length = file->get_length();
    std::unique_ptr<uint8_t[]> buffer = file->read_buffer(length);
    std::unique_ptr<Lowl::Audio::AudioData> audio_data = read(std::move(buffer), length, error);
    if (error.has_error()) {
        return nullptr;
    }
    if (!audio_data) {
        error.set_error(Lowl::ErrorCode::AudioReaderNoData);
        return nullptr;
    }
    return audio_data;
}

Lowl::Audio::AudioReader::AudioReader() {
}

std::unique_ptr<Lowl::Audio::AudioData>
Lowl::Audio::AudioReader::create_audio_data(Lowl::Audio::AudioFormat p_audio_format,
                                            Lowl::Audio::SampleFormat p_sample_format,
                                            Lowl::Audio::AudioChannel p_channel,
                                            SampleRate p_sample_rate,
                                            const std::unique_ptr<uint8_t[]> &p_buffer,
                                            size_t p_size,
                                            Lowl::Error &error) {
    const size_t sample_size = get_sample_size_bytes(p_sample_format);
    const size_t channel_count = get_channel_num(p_channel);
    if (sample_size == 0 || channel_count == 0 || p_buffer == nullptr) {
        return std::make_unique<AudioData>(std::unique_ptr<Sample[]>(), 0, p_sample_rate, p_channel);
    }

    const size_t num_samples = p_size / sample_size;
    const size_t frame_count = num_samples / channel_count;
    std::unique_ptr<Sample[]> storage;
    if (frame_count > 0) {
        storage = std::make_unique<Sample[]>(frame_count * channel_count);
    }

    auto write_channel_samples = [&](auto *p_src, auto p_convert) {
        for (size_t channel_index = 0; channel_index < channel_count; channel_index++) {
            Sample *dst = storage.get() + channel_index * frame_count;
            for (size_t frame_index = 0; frame_index < frame_count; frame_index++) {
                const size_t sample_index = frame_index * channel_count + channel_index;
                dst[frame_index] = p_convert(p_src[sample_index]);
            }
        }
    };

    auto int24_to_float = [](const uint8_t *p_bytes) -> float {
        int32_t sample = static_cast<int32_t>(p_bytes[0]) | (static_cast<int32_t>(p_bytes[1]) << 8) |
                         (static_cast<int32_t>(p_bytes[2]) << 16);
        if ((sample & 0x00800000) != 0) {
            sample |= ~0x00FFFFFF;
        }
        if (sample > 0) {
            return static_cast<float>(sample) / 0x7FFFFF;
        }
        return static_cast<float>(sample) / 0x800000;
    };

    if (p_audio_format == Lowl::Audio::AudioFormat::WAVE_FORMAT_PCM ||
        p_audio_format == Lowl::Audio::AudioFormat::WAVE_FORMAT_IEEE_FLOAT ||
        p_audio_format == Lowl::Audio::AudioFormat::MP3 || p_audio_format == Lowl::Audio::AudioFormat::FLAC) {
        switch (p_sample_format) {
            case Lowl::Audio::SampleFormat::INT_32: {
                if (p_size >= sizeof(int32_t) && storage) {
                    write_channel_samples(
                        reinterpret_cast<const int32_t *>(p_buffer.get()),
                        [](const int32_t p_sample) { return SampleConverter::int32_to_float(p_sample); });
                }
                break;
            }
            case Lowl::Audio::SampleFormat::INT_16: {
                if (p_size >= sizeof(int16_t) && storage) {
                    write_channel_samples(
                        reinterpret_cast<const int16_t *>(p_buffer.get()),
                        [](const int16_t p_sample) { return SampleConverter::int16_to_float(p_sample); });
                }
                break;
            }
            case Lowl::Audio::SampleFormat::FLOAT_32: {
                if (p_size >= sizeof(float) && storage) {
                    write_channel_samples(reinterpret_cast<const float *>(p_buffer.get()),
                                          [](const float p_sample) { return p_sample; });
                }
                break;
            }
            case Lowl::Audio::SampleFormat::FLOAT_64: {
                if (p_size >= sizeof(double) && storage) {
                    write_channel_samples(reinterpret_cast<const double *>(p_buffer.get()),
                                          [](const double p_sample) { return static_cast<Sample>(p_sample); });
                }
                break;
            }
            case Lowl::Audio::SampleFormat::INT_8: {
                if (p_size >= sizeof(int8_t) && storage) {
                    write_channel_samples(reinterpret_cast<const int8_t *>(p_buffer.get()), [](const int8_t p_sample) {
                        return SampleConverter::int8_to_float(p_sample);
                    });
                }
                break;
            }
            case Lowl::Audio::SampleFormat::U_INT_8: {
                if (p_size >= sizeof(uint8_t) && storage) {
                    write_channel_samples(
                        reinterpret_cast<const uint8_t *>(p_buffer.get()),
                        [](const uint8_t p_sample) { return SampleConverter::uint8_to_float(p_sample); });
                }
                break;
            }
            case Lowl::Audio::SampleFormat::INT_24: {
                if (p_size >= 3 && storage) {
                    const uint8_t *src = p_buffer.get();
                    for (size_t channel_index = 0; channel_index < channel_count; channel_index++) {
                        Sample *dst = storage.get() + channel_index * frame_count;
                        for (size_t frame_index = 0; frame_index < frame_count; frame_index++) {
                            const size_t sample_index = frame_index * channel_count + channel_index;
                            dst[frame_index] = int24_to_float(src + sample_index * 3);
                        }
                    }
                }
                break;
            }
            case SampleFormat::Unknown:
            default: {
                error.set_error(ErrorCode::UnsupportedAudioFormat);
                return nullptr;
            }
        }
    } else {
        error.set_error(ErrorCode::UnsupportedAudioFormat);
        return nullptr;
    }

    return std::make_unique<AudioData>(std::move(storage), frame_count, p_sample_rate, p_channel);
}

std::unique_ptr<Lowl::Audio::AudioData> Lowl::Audio::AudioReader::create_audio_data(Lowl::Audio::AudioChannel p_channel,
                                                                                    const std::vector<float> &p_samples,
                                                                                    SampleRate p_sample_rate,
                                                                                    Lowl::Error &error) {
    const size_t channel_count = get_channel_num(p_channel);
    if (channel_count == 0) {
        error.set_error(ErrorCode::UnsupportedAudioFormat);
        return nullptr;
    }

    const size_t frame_count = p_samples.size() / channel_count;
    std::unique_ptr<Sample[]> storage;
    if (frame_count > 0) {
        storage = std::make_unique<Sample[]>(frame_count * channel_count);
        for (size_t channel_index = 0; channel_index < channel_count; channel_index++) {
            Sample *dst = storage.get() + channel_index * frame_count;
            for (size_t frame_index = 0; frame_index < frame_count; frame_index++) {
                dst[frame_index] = static_cast<Sample>(p_samples[frame_index * channel_count + channel_index]);
            }
        }
    }
    return std::make_unique<AudioData>(std::move(storage), frame_count, p_sample_rate, p_channel);
}

std::unique_ptr<Lowl::Audio::AudioReader> Lowl::Audio::AudioReader::create_reader(Lowl::FileFormat format,
                                                                                  Lowl::Error &error) {
    std::unique_ptr<AudioReader> reader = std::unique_ptr<AudioReader>();
    switch (format) {
        case Lowl::FileFormat::UNKNOWN: {
            error.set_error(Lowl::ErrorCode::ReaderUnsupportedFormat);
            break;
        }
        case Lowl::FileFormat::WAV: {
            reader = std::make_unique<Lowl::Audio::AudioReaderWav>();
            break;
        }
        case Lowl::FileFormat::MP3: {
            reader = std::make_unique<Lowl::Audio::AudioReaderMp3>();
            break;
        }
        case Lowl::FileFormat::FLAC: {
            reader = std::make_unique<Lowl::Audio::AudioReaderFlac>();
            break;
        }
        case Lowl::FileFormat::OGG: {
            reader = std::make_unique<Lowl::Audio::AudioReaderOgg>();
            break;
        }
        case Lowl::FileFormat::OPUS: {
            reader = std::make_unique<Lowl::Audio::AudioReaderOpus>();
            break;
        }
    }
    return reader;
}

Lowl::FileFormat Lowl::Audio::AudioReader::detect_format(const std::string &p_path, Lowl::Error &error) {
    std::string::size_type idx;
    idx = p_path.rfind('.');
    if (idx != std::string::npos) {
        std::string extension = p_path.substr(idx + 1);
        std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);
        if (extension == "wav") {
            return Lowl::FileFormat::WAV;
        } else if (extension == "mp3") {
            return Lowl::FileFormat::MP3;
        } else if (extension == "flac") {
            return Lowl::FileFormat::FLAC;
        } else if (extension == "ogg") {
            return Lowl::FileFormat::OGG;
        } else if (extension == "opus") {
            return Lowl::FileFormat::OPUS;
        }
    }
    error.set_error(Lowl::ErrorCode::ReaderUndetectedFormat);
    return Lowl::FileFormat::UNKNOWN;
}

std::unique_ptr<Lowl::Audio::AudioData> Lowl::Audio::AudioReader::create_data(const std::string &p_path,
                                                                              Lowl::Error &error) {
    if (p_path.empty()) {
        error.set_error(Lowl::ErrorCode::ReaderEmptyPath);
        return nullptr;
    }
    FileFormat format = detect_format(p_path, error);
    if (error.has_error()) {
        return nullptr;
    }
    std::unique_ptr<Lowl::Audio::AudioReader> reader = create_reader(format, error);
    if (error.has_error()) {
        return nullptr;
    }
    if (!reader) {
        error.set_error(Lowl::ErrorCode::ReaderNotFound);
        return nullptr;
    }
    std::unique_ptr<Lowl::Audio::AudioData> audio_data = reader->read_file(p_path, error);
    if (error.has_error()) {
        return nullptr;
    }
    if (!audio_data) {
        error.set_error(Lowl::ErrorCode::ReaderNoAudioData);
        return nullptr;
    }
    audio_data->set_name(p_path);
    return audio_data;
}

std::unique_ptr<Lowl::Audio::AudioData> Lowl::Audio::AudioReader::create_data(std::unique_ptr<uint8_t[]> p_buffer,
                                                                              size_t p_size,
                                                                              Lowl::FileFormat p_format,
                                                                              Lowl::Error &error) {
    std::unique_ptr<Lowl::Audio::AudioReader> reader = create_reader(p_format, error);
    if (error.has_error()) {
        return nullptr;
    }
    if (!reader) {
        error.set_error(Lowl::ErrorCode::Error);
        return nullptr;
    }
    std::unique_ptr<Lowl::Audio::AudioData> audio_data = reader->read(std::move(p_buffer), p_size, error);
    if (error.has_error()) {
        return nullptr;
    }
    return audio_data;
}
