#include <doctest/doctest.h>

#include <lowl.h>

#include "audio/lowl_audio_buffer.h"

#include <memory>
#include <utility>
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

    std::pair<Lowl::Audio::AudioSource::RenderResult, StereoSample>
    render_one_frame(Lowl::Audio::AudioSpace &p_audio_space) {
        Lowl::Audio::AudioBuffer buffer(1, static_cast<uint8_t>(p_audio_space.get_channel_num()));
        Lowl::Audio::AudioBlockView block = buffer.view(1);
        buffer.clear(1);
        Lowl::Audio::AudioSource::RenderResult result = p_audio_space.render(block);
        StereoSample frame{};
        if (result.frames_produced > 0) {
            frame.left = block.channel(0)[0];
            frame.right = block.channel(1)[0];
        }
        return std::make_pair(result, frame);
    }
}

TEST_CASE("AudioSpace") {
    Lowl::Error error;
    Lowl::Audio::AudioSpace audio_space(44100.0, Lowl::Audio::AudioChannel::Stereo);

    SUBCASE("AudioSpace - overlapping play mixes independent voices") {
        auto audio_data = make_stereo_audio_data({
            StereoSample{0.25f, 0.50f},
            StereoSample{0.75f, -0.25f},
        });
        const Lowl::SpaceId id = audio_space.add_audio(std::move(audio_data), error);

        REQUIRE_FALSE(error.has_error());
        REQUIRE_NE(id, 0U);

        audio_space.play(id);
        audio_space.play(id);

        auto [result0, frame0] = render_one_frame(audio_space);
        REQUIRE_EQ(result0.frames_produced, 1U);
        REQUIRE_EQ(result0.state, Lowl::Audio::AudioSource::RenderState::Ok);
        REQUIRE_EQ(frame0.left, doctest::Approx(0.50f));
        REQUIRE_EQ(frame0.right, doctest::Approx(1.00f));

        auto [result1, frame1] = render_one_frame(audio_space);
        REQUIRE_EQ(result1.frames_produced, 1U);
        REQUIRE_EQ(result1.state, Lowl::Audio::AudioSource::RenderState::Ok);
        REQUIRE_EQ(frame1.left, doctest::Approx(1.50f));
        REQUIRE_EQ(frame1.right, doctest::Approx(-0.50f));

        auto [result2, frame2] = render_one_frame(audio_space);
        REQUIRE_EQ(result2.frames_produced, 0U);
        REQUIRE_EQ(result2.state, Lowl::Audio::AudioSource::RenderState::Finished);
        REQUIRE_EQ(frame2.left, doctest::Approx(0.0f));
        REQUIRE_EQ(frame2.right, doctest::Approx(0.0f));
    }

    SUBCASE("AudioSpace - clear_all_audio retires active voices safely") {
        auto audio_data = make_stereo_audio_data({StereoSample{0.5f, -0.5f}});
        const Lowl::SpaceId id = audio_space.add_audio(std::move(audio_data), error);

        REQUIRE_FALSE(error.has_error());
        REQUIRE_NE(id, 0U);

        audio_space.play(id);
        audio_space.clear_all_audio();

        auto [result, frame] = render_one_frame(audio_space);
        REQUIRE_EQ(result.frames_produced, 0U);
        REQUIRE_EQ(result.state, Lowl::Audio::AudioSource::RenderState::Finished);
        REQUIRE_EQ(frame.left, doctest::Approx(0.0f));
        REQUIRE_EQ(frame.right, doctest::Approx(0.0f));
        REQUIRE(audio_space.get_name_mapping().empty());
    }

    SUBCASE("AudioSpace - query methods ignore detached voices without cleanup side effects") {
        auto audio_data = make_stereo_audio_data({StereoSample{0.5f, -0.5f}});
        audio_data->set_name("one-shot");
        const Lowl::SpaceId id = audio_space.add_audio(std::move(audio_data), error);

        REQUIRE_FALSE(error.has_error());
        REQUIRE_NE(id, 0U);

        audio_space.play(id);

        auto [result, frame] = render_one_frame(audio_space);
        REQUIRE_EQ(result.frames_produced, 1U);
        REQUIRE_EQ(result.state, Lowl::Audio::AudioSource::RenderState::Ok);
        REQUIRE_EQ(frame.left, doctest::Approx(0.5f));
        REQUIRE_EQ(frame.right, doctest::Approx(-0.5f));

        REQUIRE_EQ(audio_space.get_frame_position(id), 0U);
        REQUIRE_EQ(audio_space.get_frames_remaining(id), 0U);
        REQUIRE_EQ(audio_space.get_frame_count(id), 1U);

        const std::map<Lowl::SpaceId, std::string> names = audio_space.get_name_mapping();
        REQUIRE_EQ(names.at(id), "one-shot");
    }
}
