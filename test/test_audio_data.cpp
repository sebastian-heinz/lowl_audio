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
            Lowl::Audio::ChannelLayout::Stereo
        );
    }

    std::unique_ptr<Lowl::Audio::AudioData>
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
        return std::make_unique<Lowl::Audio::AudioData>(std::move(storage), frame_count, 44100.0, p_layout);
    }
}

TEST_CASE("AudioData") {
    auto render_one_frame = [](Lowl::Audio::AudioVoice &p_audio_voice) {
        Lowl::Audio::AudioBuffer buffer(1, p_audio_voice.get_channel_count());
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

    SUBCASE("AudioVoice - multichannel panning only touches front left and right speakers") {
        std::shared_ptr<Lowl::Audio::AudioData> surround_audio = std::move(make_audio_data(
            Lowl::Audio::ChannelLayout::Surround_5_1, {0.5f, 0.5f, 0.25f, 0.125f, 0.75f, -0.75f}));
        Lowl::Audio::AudioVoice surround_voice(surround_audio);
        surround_voice.set_panning(1);

        Lowl::Audio::AudioBuffer buffer(1, surround_voice.get_channel_count());
        Lowl::Audio::AudioBlockView block = buffer.view(1);
        buffer.clear(1);
        Lowl::Audio::AudioSource::RenderResult result = surround_voice.render(block);

        REQUIRE_EQ(result.frames_produced, 1U);
        REQUIRE_EQ(result.state, Lowl::Audio::AudioSource::RenderState::Remove);
        REQUIRE_EQ(block.channel(0)[0], doctest::Approx(0.0f));
        REQUIRE_EQ(block.channel(1)[0], doctest::Approx(0.5f * 1.41421356f));
        REQUIRE_EQ(block.channel(2)[0], doctest::Approx(0.25f));
        REQUIRE_EQ(block.channel(3)[0], doctest::Approx(0.125f));
        REQUIRE_EQ(block.channel(4)[0], doctest::Approx(0.75f));
        REQUIRE_EQ(block.channel(5)[0], doctest::Approx(-0.75f));
    }

    SUBCASE("AudioVoice - mono panning has no effect without front left/right speakers") {
        std::shared_ptr<Lowl::Audio::AudioData> mono_audio =
            std::move(make_audio_data(Lowl::Audio::ChannelLayout::Mono, {0.5f}));
        Lowl::Audio::AudioVoice mono_voice(mono_audio);
        mono_voice.set_panning(-1);

        Lowl::Audio::AudioBuffer buffer(1, mono_voice.get_channel_count());
        Lowl::Audio::AudioBlockView block = buffer.view(1);
        buffer.clear(1);
        Lowl::Audio::AudioSource::RenderResult result = mono_voice.render(block);

        REQUIRE_EQ(result.frames_produced, 1U);
        REQUIRE_EQ(result.state, Lowl::Audio::AudioSource::RenderState::Remove);
        REQUIRE_EQ(block.channel(0)[0], doctest::Approx(0.5f));
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
            Lowl::Audio::ChannelLayout::Stereo
        );

        std::unique_ptr<Lowl::Audio::AudioData> slice = sliced_source.create_slice(0.1, 0.3);
        REQUIRE(slice != nullptr);
        REQUIRE_EQ(slice->get_frame_count(), 2U);
        REQUIRE_EQ(slice->get_channel_data(0)[0], doctest::Approx(0.30f));
        REQUIRE_EQ(slice->get_channel_data(0)[1], doctest::Approx(0.50f));
        REQUIRE_EQ(slice->get_channel_data(1)[0], doctest::Approx(0.40f));
        REQUIRE_EQ(slice->get_channel_data(1)[1], doctest::Approx(0.60f));
    }

    SUBCASE("AudioData - create_slice clamps negative begin to zero") {
        std::unique_ptr<Lowl::Sample[]> storage = std::make_unique<Lowl::Sample[]>(6);
        storage[0] = 0.10f;
        storage[1] = 0.30f;
        storage[2] = 0.50f;
        storage[3] = 0.20f;
        storage[4] = 0.40f;
        storage[5] = 0.60f;
        std::unique_ptr<Lowl::Audio::AudioData> sliced_source = std::make_unique<Lowl::Audio::AudioData>(
            std::move(storage),
            3,
            10.0,
            Lowl::Audio::ChannelLayout::Stereo
        );

        std::unique_ptr<Lowl::Audio::AudioData> slice = sliced_source->create_slice(-1.0, 0.2);
        REQUIRE(slice != nullptr);
        REQUIRE_EQ(slice->get_frame_count(), 2U);
        REQUIRE_EQ(slice->get_channel_data(0)[0], doctest::Approx(0.10f));
        REQUIRE_EQ(slice->get_channel_data(0)[1], doctest::Approx(0.30f));
        REQUIRE_EQ(slice->get_channel_data(1)[0], doctest::Approx(0.20f));
        REQUIRE_EQ(slice->get_channel_data(1)[1], doctest::Approx(0.40f));
    }

    SUBCASE("AudioVoice - seek_time clamps negative seconds to the start") {
        std::shared_ptr<Lowl::Audio::AudioData> long_audio = std::move(make_stereo_audio_data({
            StereoSample{0.10f, 0.20f},
            StereoSample{0.30f, 0.40f},
            StereoSample{0.50f, 0.60f},
        }));
        Lowl::Audio::AudioVoice voice(long_audio);

        voice.seek_time(-1.0);

        REQUIRE_EQ(voice.get_frame_position(), 0U);
    }
}
