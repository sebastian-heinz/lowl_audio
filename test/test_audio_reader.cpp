#include <doctest/doctest.h>

#include "audio/reader/lowl_audio_reader.h"
#include "audio/convert/lowl_audio_re_sampler_r8b.h"

#include <cstring>
#include <memory>
#include <vector>

namespace {
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
}
