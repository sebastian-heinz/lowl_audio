#include <doctest/doctest.h>

#include "audio/lowl_audio_utilities.h"
#include "audio/lowl_audio_buffer.h"
#include "audio/source/lowl_audio_data.h"
#include "audio/source/lowl_audio_stream.h"
#include "audio/source/lowl_audio_source.h"

#include <memory>
#include <vector>

namespace {
    struct StereoSample {
        Lowl::Sample left;
        Lowl::Sample right;
    };

    std::shared_ptr<Lowl::Audio::AudioData> make_stereo_audio_data(const std::vector<StereoSample> &p_frames) {
        const size_t frame_count = p_frames.size();
        std::unique_ptr<Lowl::Sample[]> storage;
        if (frame_count > 0) {
            storage = std::make_unique<Lowl::Sample[]>(frame_count * 2);
            for (size_t frame_index = 0; frame_index < frame_count; frame_index++) {
                storage[frame_index] = p_frames[frame_index].left;
                storage[frame_count + frame_index] = p_frames[frame_index].right;
            }
        }
        return std::make_shared<Lowl::Audio::AudioData>(
            std::move(storage),
            frame_count,
            44100.0,
            Lowl::Audio::ChannelLayout::Stereo
        );
    }
}

TEST_CASE("AudioUtilities") {
    auto render_one_frame = [](Lowl::Audio::AudioStream &p_audio_stream) {
        Lowl::Audio::AudioBuffer buffer(1, p_audio_stream.get_channel_count());
        Lowl::Audio::AudioBlockView block = buffer.view(1);
        buffer.clear(1);
        Lowl::Audio::AudioSource::RenderResult result = p_audio_stream.render(block);
        StereoSample frame{};
        if (result.frames_produced > 0) {
            frame.left = block.channel(0)[0];
            frame.right = block.channel(1)[0];
        }
        return std::make_pair(result, frame);
    };

    SUBCASE("ChannelLayout from_count maps 6 and 8 channel modes") {
        REQUIRE_EQ(Lowl::Audio::ChannelLayout::from_count(6), Lowl::Audio::ChannelLayout::Surround_5_1);
        REQUIRE_EQ(Lowl::Audio::ChannelLayout::from_count(8), Lowl::Audio::ChannelLayout::Surround_7_1);
    }

    SUBCASE("AudioUtilities - to_stream preserves more than 100 frames") {
        std::vector<StereoSample> frames;
        for (size_t frame_index = 0; frame_index < 150; frame_index++) {
            Lowl::Sample sample = static_cast<Lowl::Sample>(frame_index) / 200.0f;
            frames.push_back({sample, -sample});
        }

        std::shared_ptr<Lowl::Audio::AudioData> audio_data = make_stereo_audio_data(frames);

        Lowl::Error error;
        std::unique_ptr<Lowl::Audio::AudioStream> stream = Lowl::Audio::Utilities::to_stream(audio_data, error);
        REQUIRE_FALSE(error.has_error());
        REQUIRE(stream != nullptr);

        for (const StereoSample &expected_frame : frames) {
            auto [result, read] = render_one_frame(*stream);
            REQUIRE_EQ(result.frames_produced, 1U);
            REQUIRE_EQ(result.state, Lowl::Audio::AudioSource::RenderState::Ok);
            REQUIRE_EQ(read.left, doctest::Approx(expected_frame.left));
            REQUIRE_EQ(read.right, doctest::Approx(expected_frame.right));
        }

        auto [result, read] = render_one_frame(*stream);
        REQUIRE_EQ(result.frames_produced, 0U);
        REQUIRE_EQ(result.state, Lowl::Audio::AudioSource::RenderState::Starved);
        REQUIRE_EQ(read.left, doctest::Approx(0.0f));
        REQUIRE_EQ(read.right, doctest::Approx(0.0f));
    }

    SUBCASE("AudioUtilities - to_stream preserves 5.1 clip channel data") {
        const size_t frame_count = 3;
        const size_t channel_count = 6;
        std::unique_ptr<Lowl::Sample[]> storage = std::make_unique<Lowl::Sample[]>(frame_count * channel_count);
        for (size_t channel_index = 0; channel_index < channel_count; channel_index++) {
            for (size_t frame_index = 0; frame_index < frame_count; frame_index++) {
                storage[channel_index * frame_count + frame_index] =
                    static_cast<Lowl::Sample>(channel_index * 10 + frame_index);
            }
        }

        std::shared_ptr<Lowl::Audio::AudioData> audio_data = std::make_shared<Lowl::Audio::AudioData>(
            std::move(storage),
            frame_count,
            48000.0,
            Lowl::Audio::ChannelLayout::Surround_5_1
        );

        Lowl::Error error;
        std::unique_ptr<Lowl::Audio::AudioStream> stream = Lowl::Audio::Utilities::to_stream(audio_data, error);
        REQUIRE_FALSE(error.has_error());
        REQUIRE(stream != nullptr);

        Lowl::Audio::AudioBuffer buffer(static_cast<uint32_t>(frame_count), static_cast<uint8_t>(channel_count));
        Lowl::Audio::AudioBlockView block = buffer.view(static_cast<uint32_t>(frame_count));
        buffer.clear(block.frame_count);
        Lowl::Audio::AudioSource::RenderResult result = stream->render(block);
        REQUIRE_EQ(result.frames_produced, frame_count);
        for (uint8_t channel_index = 0; channel_index < channel_count; channel_index++) {
            for (uint32_t current_frame = 0; current_frame < frame_count; current_frame++) {
                REQUIRE_EQ(
                    block.channel(channel_index)[current_frame],
                    doctest::Approx(static_cast<float>(channel_index * 10 + current_frame))
                );
            }
        }
    }
}
