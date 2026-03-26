#include <doctest/doctest.h>

#include <lowl.h>

#include "audio/lowl_audio_buffer.h"

#include <cmath>
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

    Lowl::AudioPlaybackId add_asset_and_create_playback(Lowl::Audio::AudioSpace &p_audio_space,
                                                        std::unique_ptr<Lowl::Audio::AudioData> p_audio_data,
                                                        Lowl::Error &p_error) {
        const Lowl::AudioAssetId asset_id = p_audio_space.add_audio(std::move(p_audio_data), p_error);
        if (p_error.has_error() || asset_id == Lowl::Audio::AudioSpace::InvalidAudioAssetId) {
            return Lowl::Audio::AudioSpace::InvalidAudioPlaybackId;
        }
        return p_audio_space.create_playback(asset_id);
    }
}

TEST_CASE("AudioSpace") {
    Lowl::Error error;
    Lowl::Audio::AudioSpace audio_space(44100.0, Lowl::Audio::AudioChannel::Stereo);

    SUBCASE("AudioSpace - play restarts a playback from the beginning") {
        const Lowl::AudioPlaybackId playback_id = add_asset_and_create_playback(
            audio_space,
            make_stereo_audio_data({
                StereoSample{0.25f, 0.50f},
                StereoSample{0.75f, -0.25f},
            }),
            error
        );

        REQUIRE_FALSE(error.has_error());
        REQUIRE_NE(playback_id, Lowl::Audio::AudioSpace::InvalidAudioPlaybackId);

        audio_space.play(playback_id);

        auto [result0, frame0] = render_one_frame(audio_space);
        REQUIRE_EQ(result0.frames_produced, 1U);
        REQUIRE_EQ(result0.state, Lowl::Audio::AudioSource::RenderState::Ok);
        REQUIRE_EQ(frame0.left, doctest::Approx(0.25f));
        REQUIRE_EQ(frame0.right, doctest::Approx(0.50f));

        audio_space.play(playback_id);

        auto [result1, frame1] = render_one_frame(audio_space);
        REQUIRE_EQ(result1.frames_produced, 1U);
        REQUIRE_EQ(result1.state, Lowl::Audio::AudioSource::RenderState::Ok);
        REQUIRE_EQ(frame1.left, doctest::Approx(0.25f));
        REQUIRE_EQ(frame1.right, doctest::Approx(0.50f));

        auto [result2, frame2] = render_one_frame(audio_space);
        REQUIRE_EQ(result2.frames_produced, 1U);
        REQUIRE_EQ(result2.state, Lowl::Audio::AudioSource::RenderState::Ok);
        REQUIRE_EQ(frame2.left, doctest::Approx(0.75f));
        REQUIRE_EQ(frame2.right, doctest::Approx(-0.25f));

        auto [result3, frame3] = render_one_frame(audio_space);
        REQUIRE_EQ(result3.frames_produced, 0U);
        REQUIRE_EQ(result3.state, Lowl::Audio::AudioSource::RenderState::Finished);
        REQUIRE_EQ(frame3.left, doctest::Approx(0.0f));
        REQUIRE_EQ(frame3.right, doctest::Approx(0.0f));
    }

    SUBCASE("AudioSpace - pause and resume continue from the paused position") {
        const Lowl::AudioPlaybackId playback_id = add_asset_and_create_playback(
            audio_space,
            make_stereo_audio_data({
                StereoSample{0.25f, 0.50f},
                StereoSample{0.75f, -0.25f},
            }),
            error
        );

        REQUIRE_FALSE(error.has_error());
        REQUIRE_NE(playback_id, Lowl::Audio::AudioSpace::InvalidAudioPlaybackId);

        audio_space.play(playback_id);

        auto [result0, frame0] = render_one_frame(audio_space);
        REQUIRE_EQ(result0.frames_produced, 1U);
        REQUIRE_EQ(result0.state, Lowl::Audio::AudioSource::RenderState::Ok);
        REQUIRE_EQ(frame0.left, doctest::Approx(0.25f));
        REQUIRE_EQ(frame0.right, doctest::Approx(0.50f));

        REQUIRE_EQ(audio_space.get_frame_position(playback_id), 1U);
        REQUIRE_EQ(audio_space.get_frames_remaining(playback_id), 1U);
        REQUIRE_EQ(audio_space.get_frame_count(playback_id), 2U);

        audio_space.pause(playback_id);

        auto [paused_result, paused_frame] = render_one_frame(audio_space);
        REQUIRE_EQ(paused_result.frames_produced, 0U);
        REQUIRE_EQ(paused_result.state, Lowl::Audio::AudioSource::RenderState::Finished);
        REQUIRE_EQ(paused_frame.left, doctest::Approx(0.0f));
        REQUIRE_EQ(paused_frame.right, doctest::Approx(0.0f));

        audio_space.resume(playback_id);

        auto [result1, frame1] = render_one_frame(audio_space);
        REQUIRE_EQ(result1.frames_produced, 1U);
        REQUIRE_EQ(result1.state, Lowl::Audio::AudioSource::RenderState::Ok);
        REQUIRE_EQ(frame1.left, doctest::Approx(0.75f));
        REQUIRE_EQ(frame1.right, doctest::Approx(-0.25f));
    }

    SUBCASE("AudioSpace - resume works when pause and resume happen before the mixer processes removal") {
        const Lowl::AudioPlaybackId playback_id = add_asset_and_create_playback(
            audio_space,
            make_stereo_audio_data({
                StereoSample{0.25f, 0.50f},
                StereoSample{0.75f, -0.25f},
            }),
            error
        );

        REQUIRE_FALSE(error.has_error());
        REQUIRE_NE(playback_id, Lowl::Audio::AudioSpace::InvalidAudioPlaybackId);

        audio_space.play(playback_id);

        auto [result0, frame0] = render_one_frame(audio_space);
        REQUIRE_EQ(result0.frames_produced, 1U);
        REQUIRE_EQ(result0.state, Lowl::Audio::AudioSource::RenderState::Ok);
        REQUIRE_EQ(frame0.left, doctest::Approx(0.25f));
        REQUIRE_EQ(frame0.right, doctest::Approx(0.50f));

        audio_space.pause(playback_id);
        audio_space.resume(playback_id);

        auto [result1, frame1] = render_one_frame(audio_space);
        REQUIRE_EQ(result1.frames_produced, 1U);
        REQUIRE_EQ(result1.state, Lowl::Audio::AudioSource::RenderState::Ok);
        REQUIRE_EQ(frame1.left, doctest::Approx(0.75f));
        REQUIRE_EQ(frame1.right, doctest::Approx(-0.25f));
    }

    SUBCASE("AudioSpace - stop resets playback to the beginning") {
        const Lowl::AudioPlaybackId playback_id = add_asset_and_create_playback(
            audio_space,
            make_stereo_audio_data({
                StereoSample{0.25f, 0.50f},
                StereoSample{0.75f, -0.25f},
            }),
            error
        );

        REQUIRE_FALSE(error.has_error());
        REQUIRE_NE(playback_id, Lowl::Audio::AudioSpace::InvalidAudioPlaybackId);

        audio_space.play(playback_id);

        auto [result0, frame0] = render_one_frame(audio_space);
        REQUIRE_EQ(result0.frames_produced, 1U);
        REQUIRE_EQ(result0.state, Lowl::Audio::AudioSource::RenderState::Ok);
        REQUIRE_EQ(frame0.left, doctest::Approx(0.25f));
        REQUIRE_EQ(frame0.right, doctest::Approx(0.50f));

        audio_space.stop(playback_id);

        auto [stopped_result, stopped_frame] = render_one_frame(audio_space);
        REQUIRE_EQ(stopped_result.frames_produced, 0U);
        REQUIRE_EQ(stopped_result.state, Lowl::Audio::AudioSource::RenderState::Finished);
        REQUIRE_EQ(stopped_frame.left, doctest::Approx(0.0f));
        REQUIRE_EQ(stopped_frame.right, doctest::Approx(0.0f));

        audio_space.play(playback_id);

        auto [result1, frame1] = render_one_frame(audio_space);
        REQUIRE_EQ(result1.frames_produced, 1U);
        REQUIRE_EQ(result1.state, Lowl::Audio::AudioSource::RenderState::Ok);
        REQUIRE_EQ(frame1.left, doctest::Approx(0.25f));
        REQUIRE_EQ(frame1.right, doctest::Approx(0.50f));
    }

    SUBCASE("AudioSpace - play works when stop and play happen before the mixer processes removal") {
        const Lowl::AudioPlaybackId playback_id = add_asset_and_create_playback(
            audio_space,
            make_stereo_audio_data({
                StereoSample{0.25f, 0.50f},
                StereoSample{0.75f, -0.25f},
            }),
            error
        );

        REQUIRE_FALSE(error.has_error());
        REQUIRE_NE(playback_id, Lowl::Audio::AudioSpace::InvalidAudioPlaybackId);

        audio_space.play(playback_id);

        auto [result0, frame0] = render_one_frame(audio_space);
        REQUIRE_EQ(result0.frames_produced, 1U);
        REQUIRE_EQ(result0.state, Lowl::Audio::AudioSource::RenderState::Ok);
        REQUIRE_EQ(frame0.left, doctest::Approx(0.25f));
        REQUIRE_EQ(frame0.right, doctest::Approx(0.50f));

        audio_space.stop(playback_id);
        audio_space.play(playback_id);

        auto [result1, frame1] = render_one_frame(audio_space);
        REQUIRE_EQ(result1.frames_produced, 1U);
        REQUIRE_EQ(result1.state, Lowl::Audio::AudioSource::RenderState::Ok);
        REQUIRE_EQ(frame1.left, doctest::Approx(0.25f));
        REQUIRE_EQ(frame1.right, doctest::Approx(0.50f));
    }

    SUBCASE("AudioSpace - multiple playbacks from one asset keep independent panning") {
        const Lowl::AudioAssetId asset_id = audio_space.add_audio(
            make_stereo_audio_data({StereoSample{1.0f, 1.0f}}),
            error
        );

        REQUIRE_FALSE(error.has_error());
        REQUIRE_NE(asset_id, Lowl::Audio::AudioSpace::InvalidAudioAssetId);

        const Lowl::AudioPlaybackId playback_left = audio_space.create_playback(asset_id);
        const Lowl::AudioPlaybackId playback_right = audio_space.create_playback(asset_id);

        REQUIRE_NE(playback_left, Lowl::Audio::AudioSpace::InvalidAudioPlaybackId);
        REQUIRE_NE(playback_right, Lowl::Audio::AudioSpace::InvalidAudioPlaybackId);

        audio_space.set_panning(playback_left, -1.0f);
        audio_space.set_panning(playback_right, 1.0f);

        audio_space.play(playback_left);
        audio_space.play(playback_right);

        auto [result, frame] = render_one_frame(audio_space);
        const float pan_gain = std::sqrt(2.0f);
        REQUIRE_EQ(result.frames_produced, 1U);
        REQUIRE_EQ(result.state, Lowl::Audio::AudioSource::RenderState::Ok);
        REQUIRE_EQ(frame.left, doctest::Approx(pan_gain));
        REQUIRE_EQ(frame.right, doctest::Approx(pan_gain));
    }

    SUBCASE("AudioSpace - clear_all_audio retires active playbacks safely") {
        auto audio_data = make_stereo_audio_data({StereoSample{0.5f, -0.5f}});
        audio_data->set_name("one-shot");
        const Lowl::AudioAssetId asset_id = audio_space.add_audio(std::move(audio_data), error);

        REQUIRE_FALSE(error.has_error());
        REQUIRE_NE(asset_id, Lowl::Audio::AudioSpace::InvalidAudioAssetId);

        const Lowl::AudioPlaybackId playback_id = audio_space.create_playback(asset_id);
        REQUIRE_NE(playback_id, Lowl::Audio::AudioSpace::InvalidAudioPlaybackId);

        audio_space.play(playback_id);
        audio_space.clear_all_audio();

        auto [result, frame] = render_one_frame(audio_space);
        REQUIRE_EQ(result.frames_produced, 0U);
        REQUIRE_EQ(result.state, Lowl::Audio::AudioSource::RenderState::Finished);
        REQUIRE_EQ(frame.left, doctest::Approx(0.0f));
        REQUIRE_EQ(frame.right, doctest::Approx(0.0f));
        REQUIRE(audio_space.get_name_mapping().empty());
    }
}
