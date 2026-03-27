#include <doctest/doctest.h>

#include <lowl.h>

#include "audio/reader/lowl_audio_reader.h"
#include "audio/convert/lowl_audio_re_sampler_r8b.h"

#include <cstdio>
#include <cstring>
#include <fstream>
#include <memory>
#include <string>
#include <unistd.h>
#include <vector>

namespace {
    std::vector<uint8_t> make_minimal_wav_bytes() {
        return {
            'R','I','F','F', 38,0,0,0, 'W','A','V','E',
            'f','m','t',' ', 16,0,0,0, 1,0, 1,0,
            0x44,0xAC,0x00,0x00, 0x88,0x58,0x01,0x00, 2,0, 16,0,
            'd','a','t','a', 2,0,0,0, 0x00,0x40
        };
    }

    std::unique_ptr<uint8_t[]> copy_bytes(const std::vector<uint8_t> &p_bytes) {
        std::unique_ptr<uint8_t[]> buffer = std::make_unique<uint8_t[]>(p_bytes.size());
        std::memcpy(buffer.get(), p_bytes.data(), p_bytes.size());
        return buffer;
    }

    class TestAudioReader final : public Lowl::Audio::AudioReader {
    public:
        using AudioReader::create_audio_data;

        std::unique_ptr<Lowl::Audio::AudioData>
        read(std::unique_ptr<uint8_t[]>, size_t, Lowl::Error &) override {
            return nullptr;
        }

        bool support(Lowl::FileFormat) const override {
            return false;
        }
    };

    class ScopedTempFile {
    private:
        std::string path;

    public:
        explicit ScopedTempFile(const std::string &p_contents, const std::string &p_suffix = {}) {
            std::string pattern = "/tmp/lowl_audio_reader_XXXXXX" + p_suffix;
            std::vector<char> temp_path(pattern.begin(), pattern.end());
            temp_path.push_back('\0');

            const int fd = p_suffix.empty() ? mkstemp(temp_path.data()) : mkstemps(temp_path.data(), p_suffix.size());
            REQUIRE(fd >= 0);
            close(fd);
            path = temp_path.data();

            std::ofstream out(path, std::ios::binary);
            out.write(p_contents.data(), static_cast<std::streamsize>(p_contents.size()));
        }

        ~ScopedTempFile() {
            if (!path.empty()) {
                std::remove(path.c_str());
            }
        }

        const std::string &get_path() const {
            return path;
        }
    };
}

TEST_CASE("AudioReader") {
    TestAudioReader reader;

    SUBCASE("AudioReader - raw interleaved float32 PCM becomes planar clip data") {
        std::vector<float> samples{
            0.10f, -0.20f,
            0.30f, -0.40f,
        };
        const size_t byte_count = samples.size() * sizeof(float);
        std::unique_ptr<uint8_t[]> buffer = std::make_unique<uint8_t[]>(byte_count);
        std::memcpy(buffer.get(), samples.data(), byte_count);

        Lowl::Error error;
        std::unique_ptr<Lowl::Audio::AudioData> audio_data = reader.create_audio_data(
            Lowl::Audio::AudioFormat::WAVE_FORMAT_IEEE_FLOAT,
            Lowl::Audio::SampleFormat::FLOAT_32,
            Lowl::Audio::ChannelLayout::Stereo,
            44100.0,
            buffer,
            byte_count,
            {},
            error
        );

        REQUIRE_FALSE(error.has_error());
        REQUIRE(audio_data != nullptr);
        REQUIRE_EQ(audio_data->get_frame_count(), 2U);
        REQUIRE_EQ(audio_data->get_channel_data(0)[0], doctest::Approx(0.10f));
        REQUIRE_EQ(audio_data->get_channel_data(0)[1], doctest::Approx(0.30f));
        REQUIRE_EQ(audio_data->get_channel_data(1)[0], doctest::Approx(-0.20f));
        REQUIRE_EQ(audio_data->get_channel_data(1)[1], doctest::Approx(-0.40f));
    }

    SUBCASE("AudioReader - interleaved float samples preserve 5.1 channel planes") {
        std::vector<float> samples{
            0.0f, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f,
            6.0f, 7.0f, 8.0f, 9.0f, 10.0f, 11.0f,
        };

        Lowl::Error error;
        std::unique_ptr<Lowl::Audio::AudioData> audio_data = reader.create_audio_data(
            Lowl::Audio::ChannelLayout::Surround_5_1,
            samples,
            48000.0,
            {},
            error
        );

        REQUIRE_FALSE(error.has_error());
        REQUIRE(audio_data != nullptr);
        REQUIRE_EQ(audio_data->get_channel_count(), 6U);
        REQUIRE_EQ(audio_data->get_frame_count(), 2U);
        for (uint8_t channel_index = 0; channel_index < 6; channel_index++) {
            const Lowl::Sample *channel = audio_data->get_channel_data(channel_index);
            REQUIRE(channel != nullptr);
            REQUIRE_EQ(channel[0], doctest::Approx(static_cast<float>(channel_index)));
            REQUIRE_EQ(channel[1], doctest::Approx(static_cast<float>(channel_index + 6)));
        }
    }

    SUBCASE("ReSampler - null input returns nullptr") {
        std::unique_ptr<Lowl::Audio::AudioData> audio_data = Lowl::Audio::ReSamplerR8b::resample(nullptr, 48000.0);
        REQUIRE(audio_data == nullptr);
    }

    SUBCASE("AudioReader - detect_format sniffs WAV magic without relying on extension") {
        ScopedTempFile temp_file("RIFF1234WAVE");

        Lowl::Error error;
        const Lowl::FileFormat format = Lowl::Audio::AudioReader::detect_format(temp_file.get_path(), error);

        REQUIRE_FALSE(error.has_error());
        REQUIRE_EQ(format, Lowl::FileFormat::WAV);
    }

    SUBCASE("AudioReader - detect_format prefers FLAC magic over a misleading extension") {
        ScopedTempFile temp_file("fLaC", ".mp3");

        Lowl::Error error;
        const Lowl::FileFormat format = Lowl::Audio::AudioReader::detect_format(temp_file.get_path(), error);

        REQUIRE_FALSE(error.has_error());
        REQUIRE_EQ(format, Lowl::FileFormat::FLAC);
    }

    SUBCASE("Lib - detect_format uses the same magic-byte fallback") {
        ScopedTempFile temp_file("ID3");

        Lowl::Error error;
        const Lowl::FileFormat format = Lowl::Lib::detect_format(temp_file.get_path(), error);

        REQUIRE_FALSE(error.has_error());
        REQUIRE_EQ(format, Lowl::FileFormat::MP3);
    }

    SUBCASE("Lib - create_data decodes an in-memory WAV buffer through the facade") {
        const std::vector<uint8_t> wav_bytes = make_minimal_wav_bytes();
        Lowl::Error error;
        std::unique_ptr<Lowl::Audio::AudioData> audio_data =
            Lowl::Lib::create_data(copy_bytes(wav_bytes), wav_bytes.size(), Lowl::FileFormat::WAV, error);

        REQUIRE_FALSE(error.has_error());
        REQUIRE(audio_data != nullptr);
        REQUIRE_EQ(audio_data->get_channel_count(), 1U);
        REQUIRE_EQ(audio_data->get_frame_count(), 1U);
        REQUIRE_EQ(audio_data->get_channel_data(0)[0], doctest::Approx(0.5f).epsilon(0.01));
    }

    SUBCASE("Lib - create_data decodes an extensionless WAV file through sniffed detection") {
        const std::vector<uint8_t> wav_bytes = make_minimal_wav_bytes();
        ScopedTempFile temp_file(std::string(reinterpret_cast<const char *>(wav_bytes.data()), wav_bytes.size()));

        Lowl::Error error;
        std::unique_ptr<Lowl::Audio::AudioData> audio_data = Lowl::Lib::create_data(temp_file.get_path(), error);

        REQUIRE_FALSE(error.has_error());
        REQUIRE(audio_data != nullptr);
        REQUIRE_EQ(audio_data->get_channel_count(), 1U);
        REQUIRE_EQ(audio_data->get_frame_count(), 1U);
    }
}
