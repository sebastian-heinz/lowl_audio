#include "lowl_audio_reader.h"

#include <algorithm>
#include <cstring>
#include <memory>

#include "audio/reader/lowl_audio_reader_flac.h"
#include "audio/reader/lowl_audio_reader_mp3.h"
#include "audio/reader/lowl_audio_reader_ogg.h"
#include "audio/reader/lowl_audio_reader_opus.h"
#include "audio/reader/lowl_audio_reader_wav.h"
#include "lowl_file.h"

namespace {
    Lowl::FileFormat detect_format_from_extension(const std::string &p_path) {
        const std::string::size_type idx = p_path.rfind('.');
        if (idx == std::string::npos) {
            return Lowl::FileFormat::UNKNOWN;
        }

        std::string extension = p_path.substr(idx + 1);
        std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);
        if (extension == "wav") {
            return Lowl::FileFormat::WAV;
        }
        if (extension == "mp3") {
            return Lowl::FileFormat::MP3;
        }
        if (extension == "flac") {
            return Lowl::FileFormat::FLAC;
        }
        if (extension == "ogg") {
            return Lowl::FileFormat::OGG;
        }
        if (extension == "opus") {
            return Lowl::FileFormat::OPUS;
        }
        return Lowl::FileFormat::UNKNOWN;
    }

    bool starts_with_bytes(const uint8_t *p_buffer, const size_t p_size, const char *p_magic, const size_t p_magic_size) {
        return p_buffer != nullptr && p_size >= p_magic_size &&
               std::memcmp(p_buffer, p_magic, p_magic_size) == 0;
    }

    bool contains_bytes(const uint8_t *p_buffer, const size_t p_size, const char *p_magic, const size_t p_magic_size) {
        if (p_buffer == nullptr || p_size < p_magic_size) {
            return false;
        }
        for (size_t offset = 0; offset + p_magic_size <= p_size; offset++) {
            if (std::memcmp(p_buffer + offset, p_magic, p_magic_size) == 0) {
                return true;
            }
        }
        return false;
    }

    Lowl::FileFormat detect_format_from_magic(const uint8_t *p_buffer, const size_t p_size) {
        if (starts_with_bytes(p_buffer, p_size, "RIFF", 4) &&
            p_size >= 12 && std::memcmp(p_buffer + 8, "WAVE", 4) == 0) {
            return Lowl::FileFormat::WAV;
        }
        if (starts_with_bytes(p_buffer, p_size, "fLaC", 4)) {
            return Lowl::FileFormat::FLAC;
        }
        if (starts_with_bytes(p_buffer, p_size, "OggS", 4)) {
            return contains_bytes(p_buffer, p_size, "OpusHead", 8) ? Lowl::FileFormat::OPUS : Lowl::FileFormat::OGG;
        }
        if (starts_with_bytes(p_buffer, p_size, "ID3", 3)) {
            return Lowl::FileFormat::MP3;
        }
        if (p_buffer != nullptr && p_size >= 2 && p_buffer[0] == 0xFF && (p_buffer[1] & 0xE0u) == 0xE0u) {
            return Lowl::FileFormat::MP3;
        }
        return Lowl::FileFormat::UNKNOWN;
    }
} // namespace

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
                                            Lowl::Audio::ChannelLayout p_layout,
                                            SampleRate p_sample_rate,
                                            const std::unique_ptr<uint8_t[]> &p_buffer,
                                            size_t p_size,
                                            const std::vector<Speaker> &p_input_speakers,
                                            Lowl::Error &error) {
    const size_t sample_size = get_sample_size_bytes(p_sample_format);
    const uint8_t channel_count = p_layout.channel_count;
    if (sample_size == 0 || channel_count == 0 || p_buffer == nullptr) {
        return std::make_unique<AudioData>(std::unique_ptr<Sample[]>(), 0, p_sample_rate, p_layout);
    }

    if (!p_input_speakers.empty() && p_input_speakers.size() != channel_count) {
        error.set_error(ErrorCode::UnsupportedAudioFormat);
        return nullptr;
    }

    const size_t num_samples = p_size / sample_size;
    const size_t frame_count = num_samples / channel_count;
    std::unique_ptr<Sample[]> storage;
    if (frame_count > 0) {
        storage = std::make_unique<Sample[]>(frame_count * channel_count);
    }

    std::vector<int> source_channel_indices(channel_count, -1);
    for (uint8_t channel_index = 0; channel_index < channel_count; channel_index++) {
        if (p_input_speakers.empty()) {
            source_channel_indices[channel_index] = channel_index;
            continue;
        }

        const Speaker speaker = p_layout.speaker_at(channel_index);
        for (size_t input_index = 0; input_index < p_input_speakers.size(); input_index++) {
            if (p_input_speakers[input_index] == speaker) {
                source_channel_indices[channel_index] = static_cast<int>(input_index);
                break;
            }
        }
        if (source_channel_indices[channel_index] < 0) {
            error.set_error(ErrorCode::UnsupportedAudioFormat);
            return nullptr;
        }
    }

    auto write_channel_samples = [&](auto p_read_sample, auto p_convert) {
        for (uint8_t channel_index = 0; channel_index < channel_count; channel_index++) {
            Sample *dst = storage.get() + channel_index * frame_count;
            const size_t source_channel_index = static_cast<size_t>(source_channel_indices[channel_index]);
            for (size_t frame_index = 0; frame_index < frame_count; frame_index++) {
                const size_t sample_index = frame_index * channel_count + source_channel_index;
                dst[frame_index] = p_convert(p_read_sample(sample_index));
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
                        [&](const size_t p_sample_index) {
                            int32_t sample = 0;
                            std::memcpy(
                                &sample, p_buffer.get() + p_sample_index * sizeof(int32_t), sizeof(sample));
                            return sample;
                        },
                        [](const int32_t p_sample) { return SampleConverter::int32_to_float(p_sample); });
                }
                break;
            }
            case Lowl::Audio::SampleFormat::INT_16: {
                if (p_size >= sizeof(int16_t) && storage) {
                    write_channel_samples(
                        [&](const size_t p_sample_index) {
                            int16_t sample = 0;
                            std::memcpy(
                                &sample, p_buffer.get() + p_sample_index * sizeof(int16_t), sizeof(sample));
                            return sample;
                        },
                        [](const int16_t p_sample) { return SampleConverter::int16_to_float(p_sample); });
                }
                break;
            }
            case Lowl::Audio::SampleFormat::FLOAT_32: {
                if (p_size >= sizeof(float) && storage) {
                    write_channel_samples([&](const size_t p_sample_index) {
                                              float sample = 0.0f;
                                              std::memcpy(
                                                  &sample, p_buffer.get() + p_sample_index * sizeof(float), sizeof(sample));
                                              return sample;
                                          },
                                          [](const float p_sample) { return p_sample; });
                }
                break;
            }
            case Lowl::Audio::SampleFormat::FLOAT_64: {
                if (p_size >= sizeof(double) && storage) {
                    write_channel_samples([&](const size_t p_sample_index) {
                                              double sample = 0.0;
                                              std::memcpy(
                                                  &sample, p_buffer.get() + p_sample_index * sizeof(double), sizeof(sample));
                                              return sample;
                                          },
                                          [](const double p_sample) { return static_cast<Sample>(p_sample); });
                }
                break;
            }
            case Lowl::Audio::SampleFormat::INT_8: {
                if (p_size >= sizeof(int8_t) && storage) {
                    write_channel_samples(
                        [&](const size_t p_sample_index) {
                            int8_t sample = 0;
                            std::memcpy(&sample, p_buffer.get() + p_sample_index, sizeof(sample));
                            return sample;
                        },
                        [](const int8_t p_sample) { return SampleConverter::int8_to_float(p_sample); });
                }
                break;
            }
            case Lowl::Audio::SampleFormat::U_INT_8: {
                if (p_size >= sizeof(uint8_t) && storage) {
                    write_channel_samples(
                        [&](const size_t p_sample_index) {
                            uint8_t sample = 0;
                            std::memcpy(&sample, p_buffer.get() + p_sample_index, sizeof(sample));
                            return sample;
                        },
                        [](const uint8_t p_sample) { return SampleConverter::uint8_to_float(p_sample); });
                }
                break;
            }
            case Lowl::Audio::SampleFormat::INT_24: {
                if (p_size >= 3 && storage) {
                    const uint8_t *src = p_buffer.get();
                    for (uint8_t channel_index = 0; channel_index < channel_count; channel_index++) {
                        Sample *dst = storage.get() + channel_index * frame_count;
                        const size_t source_channel_index = static_cast<size_t>(source_channel_indices[channel_index]);
                        for (size_t frame_index = 0; frame_index < frame_count; frame_index++) {
                            const size_t sample_index = frame_index * channel_count + source_channel_index;
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

    return std::make_unique<AudioData>(std::move(storage), frame_count, p_sample_rate, p_layout);
}

std::unique_ptr<Lowl::Audio::AudioData> Lowl::Audio::AudioReader::create_audio_data(Lowl::Audio::ChannelLayout p_layout,
                                                                                    const std::vector<float> &p_samples,
                                                                                    SampleRate p_sample_rate,
                                                                                    const std::vector<Speaker> &p_input_speakers,
                                                                                    Lowl::Error &error) {
    const uint8_t channel_count = p_layout.channel_count;
    if (channel_count == 0) {
        error.set_error(ErrorCode::UnsupportedAudioFormat);
        return nullptr;
    }
    if (!p_input_speakers.empty() && p_input_speakers.size() != channel_count) {
        error.set_error(ErrorCode::UnsupportedAudioFormat);
        return nullptr;
    }

    const size_t frame_count = p_samples.size() / channel_count;
    std::unique_ptr<Sample[]> storage;
    if (frame_count > 0) {
        storage = std::make_unique<Sample[]>(frame_count * channel_count);
        for (uint8_t channel_index = 0; channel_index < channel_count; channel_index++) {
            Sample *dst = storage.get() + channel_index * frame_count;
            size_t source_channel_index = channel_index;
            if (!p_input_speakers.empty()) {
                source_channel_index = p_input_speakers.size();
                const Speaker speaker = p_layout.speaker_at(channel_index);
                for (size_t input_index = 0; input_index < p_input_speakers.size(); input_index++) {
                    if (p_input_speakers[input_index] == speaker) {
                        source_channel_index = input_index;
                        break;
                    }
                }
                if (source_channel_index >= p_input_speakers.size()) {
                    error.set_error(ErrorCode::UnsupportedAudioFormat);
                    return nullptr;
                }
            }
            for (size_t frame_index = 0; frame_index < frame_count; frame_index++) {
                dst[frame_index] =
                    static_cast<Sample>(p_samples[frame_index * channel_count + source_channel_index]);
            }
        }
    }
    return std::make_unique<AudioData>(std::move(storage), frame_count, p_sample_rate, p_layout);
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
    const Lowl::FileFormat extension_format = detect_format_from_extension(p_path);

    Lowl::File file;
    Lowl::Error file_error;
    file.open(p_path, file_error);
    if (!file_error.has_error()) {
        size_t sniff_length = 64;
        std::unique_ptr<uint8_t[]> buffer = file.read_buffer(sniff_length);
        const Lowl::FileFormat sniffed_format = detect_format_from_magic(buffer.get(), sniff_length);
        if (sniffed_format != Lowl::FileFormat::UNKNOWN) {
            return sniffed_format;
        }
    }

    if (extension_format != Lowl::FileFormat::UNKNOWN) {
        return extension_format;
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
