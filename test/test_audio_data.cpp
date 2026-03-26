#include <doctest/doctest.h>

#include <lowl.h>

#include "audio/lowl_audio_buffer.h"
#include "audio/source/lowl_audio_voice.h"

#include <memory>
#include <vector>

namespace {
    struct StereoSample {
        Lowl::Sample left;
        Lowl::Sample right;
    };

    std::unique_ptr<Lowl::Audio::AudioData> make_stereo_audio_data(const std::vector<StereoSample> &p_frames) {
        const size_t frame_count = p_frames.size();
        std::unique_ptr<Lowl::Sample[]> storage;
        if (frame_count > 0) {
            storage = std::make_unique<Lowl::Sample[]>(frame_count * 2);
            for (size_t frame_index = 0; frame_index < frame_count; frame_index++) {
                storage[frame_index] = p_frames[frame_index].left;
                storage[frame_count + frame_index] = p_frames[frame_index].right;
            }
        }
        return std::make_unique<Lowl::Audio::AudioData>(
            std::move(storage),
            frame_count,
            44100.0,
            Lowl::Audio::AudioChannel::Stereo
        );
    }
}

TEST_CASE("AudioData") {
    auto render_one_frame = [](Lowl::Audio::AudioVoice &p_audio_voice) {
        Lowl::Audio::AudioBuffer buffer(1, static_cast<uint8_t>(p_audio_voice.get_channel_num()));
        Lowl::Audio::AudioBlockView block = buffer.view(1);
        buffer.clear(1);
        Lowl::Audio::AudioSource::RenderResult result = p_audio_voice.render(block);
        StereoSample frame{};
        if (result.frames_produced > 0) {
            frame.left = block.channel(0)[0];
            frame.right = block.channel(1)[0];
        }
        return std::make_pair(result, frame);
    };

    std::shared_ptr<Lowl::Audio::AudioData> audio_data = std::move(make_stereo_audio_data({StereoSample{0.5f, 0.5f}}));
    Lowl::Audio::AudioVoice audio_voice(audio_data);

    SUBCASE("AudioVoice - Frame") {
        auto [result, read] = render_one_frame(audio_voice);
        REQUIRE_EQ(result.frames_produced, 1U);
        REQUIRE_EQ(result.state, Lowl::Audio::AudioSource::RenderState::Remove);
        REQUIRE_EQ(read.left, doctest::Approx(0.5f));
        REQUIRE_EQ(read.right, doctest::Approx(0.5f));
    }

    SUBCASE("AudioVoice - Panning") {
        auto [result0, read0] = render_one_frame(audio_voice);
        REQUIRE_EQ(result0.frames_produced, 1U);
        REQUIRE_EQ(result0.state, Lowl::Audio::AudioSource::RenderState::Remove);
        REQUIRE_EQ(read0.left, 0.5);
        REQUIRE_EQ(read0.right, 0.5);

        audio_voice.set_panning(1);
        auto [result1, read1] = render_one_frame(audio_voice);
        REQUIRE_EQ(result1.frames_produced, 1U);
        REQUIRE_EQ(result1.state, Lowl::Audio::AudioSource::RenderState::Remove);
        REQUIRE_EQ(read1.left, 0.0);
        REQUIRE_EQ(read1.right, doctest::Approx(0.70711));

        audio_voice.set_panning(-1);
        auto [result2, read2] = render_one_frame(audio_voice);
        REQUIRE_EQ(result2.frames_produced, 1U);
        REQUIRE_EQ(result2.state, Lowl::Audio::AudioSource::RenderState::Remove);
        REQUIRE_EQ(read2.left, doctest::Approx(0.70711));
        REQUIRE_EQ(read2.right, 0.0);

        audio_voice.set_panning(0);
        auto [result3, read3] = render_one_frame(audio_voice);
        REQUIRE_EQ(result3.frames_produced, 1U);
        REQUIRE_EQ(result3.state, Lowl::Audio::AudioSource::RenderState::Remove);
        REQUIRE_EQ(read3.left, 0.5);
        REQUIRE_EQ(read3.right, 0.5);
    }

    SUBCASE("AudioVoice - render rejects mismatched channel block") {
        Lowl::Audio::AudioBuffer mono_buffer(1, 1);
        Lowl::Audio::AudioBlockView mono_block = mono_buffer.view(1);
        mono_buffer.clear(1);

        Lowl::Audio::AudioSource::RenderResult result = audio_voice.render(mono_block);
        REQUIRE_EQ(result.frames_produced, 0U);
        REQUIRE_EQ(result.state, Lowl::Audio::AudioSource::RenderState::Error);
        REQUIRE_EQ(mono_block.channel(0)[0], doctest::Approx(0.0f));
    }

    SUBCASE("AudioData - stores planar samples") {
        std::unique_ptr<Lowl::Audio::AudioData> planar_audio_data = make_stereo_audio_data({
            StereoSample{0.25f, -0.50f},
            StereoSample{0.75f, 0.10f},
        });

        const Lowl::Sample *left = planar_audio_data->get_channel_data(0);
        const Lowl::Sample *right = planar_audio_data->get_channel_data(1);
        REQUIRE(left != nullptr);
        REQUIRE(right != nullptr);
        REQUIRE_EQ(left[0], doctest::Approx(0.25f));
        REQUIRE_EQ(left[1], doctest::Approx(0.75f));
        REQUIRE_EQ(right[0], doctest::Approx(-0.50f));
        REQUIRE_EQ(right[1], doctest::Approx(0.10f));
    }

    SUBCASE("AudioData - create_slice copies planar ranges") {
        std::unique_ptr<Lowl::Sample[]> storage = std::make_unique<Lowl::Sample[]>(6);
        storage[0] = 0.10f;
        storage[1] = 0.30f;
        storage[2] = 0.50f;
        storage[3] = 0.20f;
        storage[4] = 0.40f;
        storage[5] = 0.60f;
        Lowl::Audio::AudioData sliced_source(
            std::move(storage),
            3,
            10.0,
            Lowl::Audio::AudioChannel::Stereo
        );

        std::unique_ptr<Lowl::Audio::AudioData> slice = sliced_source.create_slice(0.1, 0.3);
        REQUIRE(slice != nullptr);
        REQUIRE_EQ(slice->get_frame_count(), 2U);
        REQUIRE_EQ(slice->get_channel_data(0)[0], doctest::Approx(0.30f));
        REQUIRE_EQ(slice->get_channel_data(0)[1], doctest::Approx(0.50f));
        REQUIRE_EQ(slice->get_channel_data(1)[0], doctest::Approx(0.40f));
        REQUIRE_EQ(slice->get_channel_data(1)[1], doctest::Approx(0.60f));
    }
}
