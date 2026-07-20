#include <doctest/doctest.h>

#include "audio/convert/lowl_audio_channel_converter.h"

#include <memory>
#include <vector>

namespace {
    std::shared_ptr<Lowl::Audio::AudioData>
    make_audio_data(Lowl::Audio::ChannelLayout p_layout, const std::vector<Lowl::Sample> &p_interleaved_frames) {
        const uint8_t channel_count = p_layout.channel_count;
        const size_t frame_count = channel_count == 0 ? 0 : p_interleaved_frames.size() / channel_count;
        std::unique_ptr<Lowl::Sample[]> storage;
        if (frame_count > 0 && channel_count > 0) {
            storage = std::make_unique<Lowl::Sample[]>(frame_count * channel_count);
            for (size_t frame_index = 0; frame_index < frame_count; frame_index++) {
                for (uint8_t channel_index = 0; channel_index < channel_count; channel_index++) {
                    storage[static_cast<size_t>(channel_index) * frame_count + frame_index] =
                        p_interleaved_frames[frame_index * channel_count + channel_index];
                }
            }
        }
        return std::make_shared<Lowl::Audio::AudioData>(
            std::move(storage), frame_count, Lowl::Audio::AudioFormat{48000.0, p_layout});
    }
} // namespace

TEST_CASE("ChannelConverter") {
    using Lowl::Audio::ChannelConverter;
    using Lowl::Audio::ChannelLayout;
    using Lowl::Audio::Speaker;

    ChannelConverter converter;

    SUBCASE("same layout conversion copies audio data") {
        auto source = make_audio_data(ChannelLayout::Stereo, {0.25f, -0.25f, 0.5f, -0.5f});
        Lowl::Error error;
        std::unique_ptr<Lowl::Audio::AudioData> converted = converter.convert(ChannelLayout::Stereo, source, error);
        REQUIRE_FALSE(error.has_error());
        REQUIRE(converted != nullptr);
        REQUIRE_EQ(converted->get_audio_format().channel_layout, ChannelLayout::Stereo);
        REQUIRE_EQ(converted->get_channel_data(0)[0], doctest::Approx(0.25f));
        REQUIRE_EQ(converted->get_channel_data(1)[1], doctest::Approx(-0.5f));
    }

    SUBCASE("mono stereo conversions duplicate and average channels") {
        auto mono = make_audio_data(ChannelLayout::Mono, {0.4f, -0.2f});
        Lowl::Error mono_to_stereo_error;
        std::unique_ptr<Lowl::Audio::AudioData> stereo =
            converter.convert(ChannelLayout::Stereo, mono, mono_to_stereo_error);
        REQUIRE_FALSE(mono_to_stereo_error.has_error());
        REQUIRE_EQ(stereo->get_channel_count(), 2U);
        REQUIRE_EQ(stereo->get_channel_data(0)[0], doctest::Approx(0.4f));
        REQUIRE_EQ(stereo->get_channel_data(1)[1], doctest::Approx(-0.2f));

        auto stereo_source = make_audio_data(ChannelLayout::Stereo, {0.5f, -0.5f, 0.1f, 0.3f});
        Lowl::Error stereo_to_mono_error;
        std::unique_ptr<Lowl::Audio::AudioData> mono_result =
            converter.convert(ChannelLayout::Mono, stereo_source, stereo_to_mono_error);
        REQUIRE_FALSE(stereo_to_mono_error.has_error());
        REQUIRE_EQ(mono_result->get_channel_data(0)[0], doctest::Approx(0.0f));
        REQUIRE_EQ(mono_result->get_channel_data(0)[1], doctest::Approx(0.2f));
    }

    SUBCASE("5.1 side to rear remaps surround channels") {
        auto source = make_audio_data(ChannelLayout::Surround_5_1, {1, 2, 3, 4, 5, 6});
        Lowl::Error error;
        std::unique_ptr<Lowl::Audio::AudioData> converted =
            converter.convert(ChannelLayout::Surround_5_1_Rear, source, error);
        REQUIRE_FALSE(error.has_error());
        REQUIRE(converted != nullptr);
        REQUIRE_EQ(converted->get_audio_format().channel_layout, ChannelLayout::Surround_5_1_Rear);
        REQUIRE_EQ(converted->get_channel_data(0)[0], doctest::Approx(1.0f));
        REQUIRE_EQ(converted->get_channel_data(1)[0], doctest::Approx(2.0f));
        REQUIRE_EQ(converted->get_channel_data(2)[0], doctest::Approx(3.0f));
        REQUIRE_EQ(converted->get_channel_data(3)[0], doctest::Approx(4.0f));
        REQUIRE_EQ(converted->get_channel_data(4)[0], doctest::Approx(5.0f));
        REQUIRE_EQ(converted->get_channel_data(5)[0], doctest::Approx(6.0f));
    }

    SUBCASE("5.1 downmix and stereo upmix use the expected speaker routing") {
        auto surround = make_audio_data(ChannelLayout::Surround_5_1, {1, 2, 3, 4, 5, 6});
        Lowl::Error downmix_error;
        std::unique_ptr<Lowl::Audio::AudioData> stereo =
            converter.convert(ChannelLayout::Stereo, surround, downmix_error);
        REQUIRE_FALSE(downmix_error.has_error());
        REQUIRE_EQ(stereo->get_channel_data(0)[0], doctest::Approx(1.0f + 3.0f * 0.70710678f + 5.0f * 0.70710678f));
        REQUIRE_EQ(stereo->get_channel_data(1)[0], doctest::Approx(2.0f + 3.0f * 0.70710678f + 6.0f * 0.70710678f));

        auto stereo_source = make_audio_data(ChannelLayout::Stereo, {0.2f, -0.2f});
        Lowl::Error upmix_error;
        std::unique_ptr<Lowl::Audio::AudioData> surround_result =
            converter.convert(ChannelLayout::Surround_5_1, stereo_source, upmix_error);
        REQUIRE_FALSE(upmix_error.has_error());
        REQUIRE_EQ(surround_result->get_channel_data(0)[0], doctest::Approx(0.2f));
        REQUIRE_EQ(surround_result->get_channel_data(1)[0], doctest::Approx(-0.2f));
        REQUIRE_EQ(surround_result->get_channel_data(2)[0], doctest::Approx(0.0f));
        REQUIRE_EQ(surround_result->get_channel_data(3)[0], doctest::Approx(0.0f));
        REQUIRE_EQ(surround_result->get_channel_data(4)[0], doctest::Approx(0.0f));
        REQUIRE_EQ(surround_result->get_channel_data(5)[0], doctest::Approx(0.0f));
    }

    SUBCASE("unsupported speaker layouts report conversion errors") {
        const Lowl::Audio::ChannelLayout unsupported_layout = Lowl::Audio::ChannelLayout::from_mask(
            Lowl::Audio::speaker_bits(Speaker::FrontLeft) | Lowl::Audio::speaker_bits(Speaker::TopFrontLeft));
        auto source = make_audio_data(unsupported_layout, {0.1f, 0.2f});
        Lowl::Error error;
        std::unique_ptr<Lowl::Audio::AudioData> converted = converter.convert(ChannelLayout::Stereo, source, error);
        REQUIRE(converted == nullptr);
        REQUIRE(error.has_error());
        REQUIRE_EQ(error.get_error_code(), static_cast<int>(Lowl::ErrorCode::ConvertAudioChannelNotSupported));
    }
}
