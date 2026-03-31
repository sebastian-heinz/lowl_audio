#include <doctest/doctest.h>

#include <lowl.h>

#include "audio/lowl_audio_buffer.h"

#include <cmath>
#include <limits>
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
            Lowl::Audio::ChannelLayout::Stereo
        );
    }

    std::pair<Lowl::Audio::AudioSource::RenderResult, StereoSample>
    render_one_frame(Lowl::Audio::AudioSpace &p_audio_space) {
        Lowl::Audio::AudioBuffer buffer(1, p_audio_space.get_channel_count());
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

    Lowl::AudioPlaybackHandle add_asset_and_create_playback(Lowl::Audio::AudioSpace &p_audio_space,
                                                            std::unique_ptr<Lowl::Audio::AudioData> p_audio_data,
                                                            Lowl::Error &p_error) {
        const Lowl::AudioAssetHandle asset_handle = p_audio_space.add_audio(std::move(p_audio_data), p_error);
        if (p_error.has_error() || !asset_handle.is_valid()) {
            return Lowl::Audio::AudioSpace::InvalidAudioPlaybackHandle;
        }
        return p_audio_space.create_playback(asset_handle);
    }
}

TEST_CASE("AudioSpace") {
    Lowl::Error error;
    Lowl::Audio::AudioSpace audio_space(44100.0, Lowl::Audio::ChannelLayout::Stereo);

    SUBCASE("AudioSpace - aggregate frame queries use a consistent live sentinel") {
        REQUIRE_EQ(audio_space.get_frames_remaining(), 1U);
        REQUIRE_EQ(audio_space.get_frame_count(), 1U);
        REQUIRE_EQ(audio_space.get_frame_position(), 0U);
    }

    SUBCASE("AudioSpace - play restarts a playback from the beginning") {
        const Lowl::AudioPlaybackHandle playback_handle = add_asset_and_create_playback(
            audio_space,
            make_stereo_audio_data({
                StereoSample{0.25f, 0.50f},
                StereoSample{0.75f, -0.25f},
            }),
            error
        );

        REQUIRE_FALSE(error.has_error());
        REQUIRE(playback_handle.is_valid());

        audio_space.play(playback_handle);

        auto [result0, frame0] = render_one_frame(audio_space);
        REQUIRE_EQ(result0.frames_produced, 1U);
        REQUIRE_EQ(result0.state, Lowl::Audio::AudioSource::RenderState::Ok);
        REQUIRE_EQ(frame0.left, doctest::Approx(0.25f));
        REQUIRE_EQ(frame0.right, doctest::Approx(0.50f));

        audio_space.play(playback_handle);

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

    SUBCASE("AudioSpace - base AudioSource pause gates render without advancing playback") {
        const Lowl::AudioPlaybackHandle playback_handle = add_asset_and_create_playback(
            audio_space,
            make_stereo_audio_data({
                StereoSample{0.25f, 0.50f},
                StereoSample{0.75f, -0.25f},
            }),
            error
        );

        REQUIRE_FALSE(error.has_error());
        REQUIRE(playback_handle.is_valid());

        audio_space.play(playback_handle);

        auto [result0, frame0] = render_one_frame(audio_space);
        REQUIRE_EQ(result0.frames_produced, 1U);
        REQUIRE_EQ(result0.state, Lowl::Audio::AudioSource::RenderState::Ok);
        REQUIRE_EQ(frame0.left, doctest::Approx(0.25f));
        REQUIRE_EQ(frame0.right, doctest::Approx(0.50f));
        REQUIRE_EQ(audio_space.get_frame_position(playback_handle), 1U);

        Lowl::Audio::AudioSource &source = audio_space;
        source.pause();
        REQUIRE(source.is_pause());

        auto [paused_result, paused_frame] = render_one_frame(audio_space);
        REQUIRE_EQ(paused_result.frames_produced, 0U);
        REQUIRE_EQ(paused_result.state, Lowl::Audio::AudioSource::RenderState::Starved);
        REQUIRE_EQ(paused_frame.left, doctest::Approx(0.0f));
        REQUIRE_EQ(paused_frame.right, doctest::Approx(0.0f));
        REQUIRE_EQ(audio_space.get_frame_position(playback_handle), 1U);

        source.play();
        REQUIRE(source.is_play());

        auto [result1, frame1] = render_one_frame(audio_space);
        REQUIRE_EQ(result1.frames_produced, 1U);
        REQUIRE_EQ(result1.state, Lowl::Audio::AudioSource::RenderState::Ok);
        REQUIRE_EQ(frame1.left, doctest::Approx(0.75f));
        REQUIRE_EQ(frame1.right, doctest::Approx(-0.25f));
    }

    SUBCASE("AudioSpace - pause and resume continue from the paused position") {
        const Lowl::AudioPlaybackHandle playback_handle = add_asset_and_create_playback(
            audio_space,
            make_stereo_audio_data({
                StereoSample{0.25f, 0.50f},
                StereoSample{0.75f, -0.25f},
            }),
            error
        );

        REQUIRE_FALSE(error.has_error());
        REQUIRE(playback_handle.is_valid());

        audio_space.play(playback_handle);

        auto [result0, frame0] = render_one_frame(audio_space);
        REQUIRE_EQ(result0.frames_produced, 1U);
        REQUIRE_EQ(result0.state, Lowl::Audio::AudioSource::RenderState::Ok);
        REQUIRE_EQ(frame0.left, doctest::Approx(0.25f));
        REQUIRE_EQ(frame0.right, doctest::Approx(0.50f));

        REQUIRE_EQ(audio_space.get_frame_position(playback_handle), 1U);
        REQUIRE_EQ(audio_space.get_frames_remaining(playback_handle), 1U);
        REQUIRE_EQ(audio_space.get_frame_count(playback_handle), 2U);

        audio_space.pause(playback_handle);

        auto [paused_result, paused_frame] = render_one_frame(audio_space);
        REQUIRE_EQ(paused_result.frames_produced, 0U);
        REQUIRE_EQ(paused_result.state, Lowl::Audio::AudioSource::RenderState::Finished);
        REQUIRE_EQ(paused_frame.left, doctest::Approx(0.0f));
        REQUIRE_EQ(paused_frame.right, doctest::Approx(0.0f));

        audio_space.resume(playback_handle);

        auto [result1, frame1] = render_one_frame(audio_space);
        REQUIRE_EQ(result1.frames_produced, 1U);
        REQUIRE_EQ(result1.state, Lowl::Audio::AudioSource::RenderState::Ok);
        REQUIRE_EQ(frame1.left, doctest::Approx(0.75f));
        REQUIRE_EQ(frame1.right, doctest::Approx(-0.25f));
    }

    SUBCASE("AudioSpace - resume works when pause and resume happen before the mixer processes removal") {
        const Lowl::AudioPlaybackHandle playback_handle = add_asset_and_create_playback(
            audio_space,
            make_stereo_audio_data({
                StereoSample{0.25f, 0.50f},
                StereoSample{0.75f, -0.25f},
            }),
            error
        );

        REQUIRE_FALSE(error.has_error());
        REQUIRE(playback_handle.is_valid());

        audio_space.play(playback_handle);

        auto [result0, frame0] = render_one_frame(audio_space);
        REQUIRE_EQ(result0.frames_produced, 1U);
        REQUIRE_EQ(result0.state, Lowl::Audio::AudioSource::RenderState::Ok);
        REQUIRE_EQ(frame0.left, doctest::Approx(0.25f));
        REQUIRE_EQ(frame0.right, doctest::Approx(0.50f));

        audio_space.pause(playback_handle);
        audio_space.resume(playback_handle);

        auto [result1, frame1] = render_one_frame(audio_space);
        REQUIRE_EQ(result1.frames_produced, 1U);
        REQUIRE_EQ(result1.state, Lowl::Audio::AudioSource::RenderState::Ok);
        REQUIRE_EQ(frame1.left, doctest::Approx(0.75f));
        REQUIRE_EQ(frame1.right, doctest::Approx(-0.25f));
    }

    SUBCASE("AudioSpace - stop resets playback to the beginning") {
        const Lowl::AudioPlaybackHandle playback_handle = add_asset_and_create_playback(
            audio_space,
            make_stereo_audio_data({
                StereoSample{0.25f, 0.50f},
                StereoSample{0.75f, -0.25f},
            }),
            error
        );

        REQUIRE_FALSE(error.has_error());
        REQUIRE(playback_handle.is_valid());

        audio_space.play(playback_handle);

        auto [result0, frame0] = render_one_frame(audio_space);
        REQUIRE_EQ(result0.frames_produced, 1U);
        REQUIRE_EQ(result0.state, Lowl::Audio::AudioSource::RenderState::Ok);
        REQUIRE_EQ(frame0.left, doctest::Approx(0.25f));
        REQUIRE_EQ(frame0.right, doctest::Approx(0.50f));

        audio_space.stop(playback_handle);

        auto [stopped_result, stopped_frame] = render_one_frame(audio_space);
        REQUIRE_EQ(stopped_result.frames_produced, 0U);
        REQUIRE_EQ(stopped_result.state, Lowl::Audio::AudioSource::RenderState::Finished);
        REQUIRE_EQ(stopped_frame.left, doctest::Approx(0.0f));
        REQUIRE_EQ(stopped_frame.right, doctest::Approx(0.0f));

        audio_space.play(playback_handle);

        auto [result1, frame1] = render_one_frame(audio_space);
        REQUIRE_EQ(result1.frames_produced, 1U);
        REQUIRE_EQ(result1.state, Lowl::Audio::AudioSource::RenderState::Ok);
        REQUIRE_EQ(frame1.left, doctest::Approx(0.25f));
        REQUIRE_EQ(frame1.right, doctest::Approx(0.50f));
    }

    SUBCASE("AudioSpace - seek and reset update query state immediately") {
        const Lowl::AudioPlaybackHandle playback_handle = add_asset_and_create_playback(
            audio_space,
            make_stereo_audio_data({
                StereoSample{0.25f, 0.50f},
                StereoSample{0.75f, -0.25f},
                StereoSample{-0.50f, 0.125f},
                StereoSample{0.125f, -0.75f},
            }),
            error
        );

        REQUIRE_FALSE(error.has_error());
        REQUIRE(playback_handle.is_valid());

        audio_space.play(playback_handle);

        auto [result0, frame0] = render_one_frame(audio_space);
        REQUIRE_EQ(result0.frames_produced, 1U);
        REQUIRE_EQ(result0.state, Lowl::Audio::AudioSource::RenderState::Ok);
        REQUIRE_EQ(frame0.left, doctest::Approx(0.25f));
        REQUIRE_EQ(frame0.right, doctest::Approx(0.50f));
        REQUIRE_EQ(audio_space.get_frame_position(playback_handle), 1U);
        REQUIRE_EQ(audio_space.get_frames_remaining(playback_handle), 3U);

        audio_space.seek_frame(playback_handle, 2);

        REQUIRE_EQ(audio_space.get_frame_position(playback_handle), 2U);
        REQUIRE_EQ(audio_space.get_frames_remaining(playback_handle), 2U);

        auto [result1, frame1] = render_one_frame(audio_space);
        REQUIRE_EQ(result1.frames_produced, 1U);
        REQUIRE_EQ(result1.state, Lowl::Audio::AudioSource::RenderState::Ok);
        REQUIRE_EQ(frame1.left, doctest::Approx(-0.50f));
        REQUIRE_EQ(frame1.right, doctest::Approx(0.125f));
        REQUIRE_EQ(audio_space.get_frame_position(playback_handle), 3U);
        REQUIRE_EQ(audio_space.get_frames_remaining(playback_handle), 1U);

        audio_space.reset(playback_handle);

        REQUIRE_EQ(audio_space.get_frame_position(playback_handle), 0U);
        REQUIRE_EQ(audio_space.get_frames_remaining(playback_handle), 4U);

        auto [result2, frame2] = render_one_frame(audio_space);
        REQUIRE_EQ(result2.frames_produced, 1U);
        REQUIRE_EQ(result2.state, Lowl::Audio::AudioSource::RenderState::Ok);
        REQUIRE_EQ(frame2.left, doctest::Approx(0.25f));
        REQUIRE_EQ(frame2.right, doctest::Approx(0.50f));
    }

    SUBCASE("AudioSpace - play works when stop and play happen before the mixer processes removal") {
        const Lowl::AudioPlaybackHandle playback_handle = add_asset_and_create_playback(
            audio_space,
            make_stereo_audio_data({
                StereoSample{0.25f, 0.50f},
                StereoSample{0.75f, -0.25f},
            }),
            error
        );

        REQUIRE_FALSE(error.has_error());
        REQUIRE(playback_handle.is_valid());

        audio_space.play(playback_handle);

        auto [result0, frame0] = render_one_frame(audio_space);
        REQUIRE_EQ(result0.frames_produced, 1U);
        REQUIRE_EQ(result0.state, Lowl::Audio::AudioSource::RenderState::Ok);
        REQUIRE_EQ(frame0.left, doctest::Approx(0.25f));
        REQUIRE_EQ(frame0.right, doctest::Approx(0.50f));

        audio_space.stop(playback_handle);
        audio_space.play(playback_handle);

        auto [result1, frame1] = render_one_frame(audio_space);
        REQUIRE_EQ(result1.frames_produced, 1U);
        REQUIRE_EQ(result1.state, Lowl::Audio::AudioSource::RenderState::Ok);
        REQUIRE_EQ(frame1.left, doctest::Approx(0.25f));
        REQUIRE_EQ(frame1.right, doctest::Approx(0.50f));
    }

    SUBCASE("AudioSpace - multiple playbacks from one asset keep independent panning") {
        const Lowl::AudioAssetHandle asset_handle = audio_space.add_audio(
            make_stereo_audio_data({StereoSample{1.0f, 1.0f}}),
            error
        );

        REQUIRE_FALSE(error.has_error());
        REQUIRE(asset_handle.is_valid());

        const Lowl::AudioPlaybackHandle playback_left = audio_space.create_playback(asset_handle);
        const Lowl::AudioPlaybackHandle playback_right = audio_space.create_playback(asset_handle);

        REQUIRE(playback_left.is_valid());
        REQUIRE(playback_right.is_valid());

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

    SUBCASE("AudioSpace - base volume scales the mixed output once") {
        const Lowl::AudioPlaybackHandle playback_handle = add_asset_and_create_playback(
            audio_space,
            make_stereo_audio_data({StereoSample{0.5f, 0.25f}}),
            error
        );

        REQUIRE_FALSE(error.has_error());
        REQUIRE(playback_handle.is_valid());

        static_cast<Lowl::Audio::AudioSource &>(audio_space).set_volume(0.5f);
        audio_space.play(playback_handle);

        auto [result, frame] = render_one_frame(audio_space);
        REQUIRE_EQ(result.frames_produced, 1U);
        REQUIRE_EQ(result.state, Lowl::Audio::AudioSource::RenderState::Ok);
        REQUIRE_EQ(frame.left, doctest::Approx(0.25f));
        REQUIRE_EQ(frame.right, doctest::Approx(0.125f));
    }

    SUBCASE("AudioSpace - base panning applies to the final mixed block") {
        const Lowl::AudioPlaybackHandle playback_handle = add_asset_and_create_playback(
            audio_space,
            make_stereo_audio_data({StereoSample{0.5f, 0.5f}}),
            error
        );

        REQUIRE_FALSE(error.has_error());
        REQUIRE(playback_handle.is_valid());

        static_cast<Lowl::Audio::AudioSource &>(audio_space).set_panning(1.0f);
        audio_space.play(playback_handle);

        auto [result, frame] = render_one_frame(audio_space);
        REQUIRE_EQ(result.frames_produced, 1U);
        REQUIRE_EQ(result.state, Lowl::Audio::AudioSource::RenderState::Ok);
        REQUIRE_EQ(frame.left, doctest::Approx(0.0f));
        REQUIRE_EQ(frame.right, doctest::Approx(0.5f * std::sqrt(2.0f)));
    }

    SUBCASE("AudioSpace - nested buses compose gain for routed playbacks") {
        const Lowl::AudioAssetHandle asset_handle = audio_space.add_audio(
            make_stereo_audio_data({StereoSample{1.0f, 1.0f}}),
            error
        );

        REQUIRE_FALSE(error.has_error());
        REQUIRE(asset_handle.is_valid());

        const Lowl::AudioBusHandle parent_bus = audio_space.create_bus(audio_space.master_bus());
        const Lowl::AudioBusHandle child_bus = audio_space.create_bus(parent_bus);

        REQUIRE(parent_bus.is_valid());
        REQUIRE(child_bus.is_valid());

        const Lowl::AudioPlaybackHandle playback_handle = audio_space.create_playback(asset_handle, child_bus);
        REQUIRE(playback_handle.is_valid());

        audio_space.set_volume(parent_bus, 0.5f);
        audio_space.set_volume(child_bus, 0.25f);
        audio_space.play(playback_handle);

        auto [result, frame] = render_one_frame(audio_space);
        REQUIRE_EQ(result.frames_produced, 1U);
        REQUIRE_EQ(result.state, Lowl::Audio::AudioSource::RenderState::Ok);
        REQUIRE_EQ(frame.left, doctest::Approx(0.125f));
        REQUIRE_EQ(frame.right, doctest::Approx(0.125f));
    }

    SUBCASE("AudioSpace - empty bus slots are reused with a new generation after destroy") {
        const Lowl::AudioBusHandle first_bus = audio_space.create_bus(audio_space.master_bus());
        REQUIRE(first_bus.is_valid());

        audio_space.destroy_bus(first_bus);

        auto [destroy_result, destroy_frame] = render_one_frame(audio_space);
        REQUIRE_EQ(destroy_result.frames_produced, 0U);
        REQUIRE_EQ(destroy_result.state, Lowl::Audio::AudioSource::RenderState::Finished);
        REQUIRE_EQ(destroy_frame.left, doctest::Approx(0.0f));
        REQUIRE_EQ(destroy_frame.right, doctest::Approx(0.0f));

        const Lowl::AudioBusHandle second_bus = audio_space.create_bus(audio_space.master_bus());
        REQUIRE(second_bus.is_valid());
        REQUIRE_EQ(second_bus.id, first_bus.id);
        REQUIRE_NE(second_bus.generation, first_bus.generation);
    }

    SUBCASE("AudioSpace - foreign bus handles are rejected across spaces") {
        Lowl::Error other_error;
        Lowl::Audio::AudioSpace other_audio_space(44100.0, Lowl::Audio::ChannelLayout::Stereo);

        const Lowl::AudioAssetHandle asset_handle = audio_space.add_audio(
            make_stereo_audio_data({StereoSample{0.25f, -0.5f}}),
            error
        );

        REQUIRE_FALSE(error.has_error());
        REQUIRE_FALSE(other_error.has_error());
        REQUIRE(asset_handle.is_valid());

        const Lowl::AudioBusHandle own_bus = audio_space.create_bus(audio_space.master_bus());
        const Lowl::AudioBusHandle foreign_bus = other_audio_space.create_bus(other_audio_space.master_bus());

        REQUIRE(own_bus.is_valid());
        REQUIRE(foreign_bus.is_valid());
        REQUIRE_EQ(own_bus.id, foreign_bus.id);
        REQUIRE_EQ(own_bus.generation, foreign_bus.generation);
        REQUIRE_NE(own_bus.owner_id, foreign_bus.owner_id);

        REQUIRE_EQ(audio_space.create_bus(foreign_bus), Lowl::Audio::AudioSpace::InvalidAudioBusHandle);
        REQUIRE_EQ(audio_space.create_playback(asset_handle, foreign_bus),
                   Lowl::Audio::AudioSpace::InvalidAudioPlaybackHandle);
        REQUIRE_EQ(audio_space.play_clip(asset_handle, foreign_bus),
                   Lowl::Audio::AudioSpace::InvalidAudioPlaybackHandle);
    }

    SUBCASE("AudioSpace - clear_all_audio retires active playbacks safely") {
        auto audio_data = make_stereo_audio_data({StereoSample{0.5f, -0.5f}});
        audio_data->set_name("one-shot");
        const Lowl::AudioAssetHandle asset_handle = audio_space.add_audio(std::move(audio_data), error);

        REQUIRE_FALSE(error.has_error());
        REQUIRE(asset_handle.is_valid());

        const Lowl::AudioPlaybackHandle playback_handle = audio_space.create_playback(asset_handle);
        REQUIRE(playback_handle.is_valid());

        audio_space.play(playback_handle);
        audio_space.clear_all_audio();

        auto [result, frame] = render_one_frame(audio_space);
        REQUIRE_EQ(result.frames_produced, 0U);
        REQUIRE_EQ(result.state, Lowl::Audio::AudioSource::RenderState::Finished);
        REQUIRE_EQ(frame.left, doctest::Approx(0.0f));
        REQUIRE_EQ(frame.right, doctest::Approx(0.0f));
        REQUIRE(audio_space.get_name_mapping().empty());
    }

    SUBCASE("AudioSpace - destroy_playback immediately reuses an unsubmitted slot") {
        const Lowl::AudioAssetHandle asset_handle = audio_space.add_audio(
            make_stereo_audio_data({StereoSample{0.5f, -0.25f}}),
            error
        );

        REQUIRE_FALSE(error.has_error());
        REQUIRE(asset_handle.is_valid());

        const Lowl::AudioPlaybackHandle first_playback = audio_space.create_playback(asset_handle);
        REQUIRE(first_playback.is_valid());

        audio_space.destroy_playback(first_playback);

        REQUIRE_EQ(audio_space.get_frame_count(first_playback), 0U);

        const Lowl::AudioPlaybackHandle second_playback = audio_space.create_playback(asset_handle);
        REQUIRE(second_playback.is_valid());
        REQUIRE_EQ(second_playback.id, first_playback.id);
        REQUIRE_NE(second_playback.generation, first_playback.generation);

        audio_space.play(first_playback);

        auto [stale_result, stale_frame] = render_one_frame(audio_space);
        REQUIRE_EQ(stale_result.frames_produced, 0U);
        REQUIRE_EQ(stale_result.state, Lowl::Audio::AudioSource::RenderState::Finished);
        REQUIRE_EQ(stale_frame.left, doctest::Approx(0.0f));
        REQUIRE_EQ(stale_frame.right, doctest::Approx(0.0f));

        audio_space.play(second_playback);

        auto [reused_result, reused_frame] = render_one_frame(audio_space);
        REQUIRE_EQ(reused_result.frames_produced, 1U);
        REQUIRE_EQ(reused_result.state, Lowl::Audio::AudioSource::RenderState::Ok);
        REQUIRE_EQ(reused_frame.left, doctest::Approx(0.5f));
        REQUIRE_EQ(reused_frame.right, doctest::Approx(-0.25f));
    }

    SUBCASE("AudioSpace - remove_audio retires a single asset without interrupting existing playbacks") {
        const Lowl::AudioAssetHandle first_asset_handle = audio_space.add_audio(
            make_stereo_audio_data({StereoSample{0.25f, -0.5f}}),
            error
        );

        REQUIRE_FALSE(error.has_error());
        REQUIRE(first_asset_handle.is_valid());

        const Lowl::AudioPlaybackHandle playback_handle = audio_space.create_playback(first_asset_handle);
        REQUIRE(playback_handle.is_valid());

        audio_space.play(playback_handle);
        audio_space.remove_audio(first_asset_handle);

        REQUIRE_EQ(audio_space.create_playback(first_asset_handle), Lowl::Audio::AudioSpace::InvalidAudioPlaybackHandle);

        auto [result, frame] = render_one_frame(audio_space);
        REQUIRE_EQ(result.frames_produced, 1U);
        REQUIRE_EQ(result.state, Lowl::Audio::AudioSource::RenderState::Ok);
        REQUIRE_EQ(frame.left, doctest::Approx(0.25f));
        REQUIRE_EQ(frame.right, doctest::Approx(-0.5f));

        const Lowl::AudioAssetHandle second_asset_handle = audio_space.add_audio(
            make_stereo_audio_data({StereoSample{0.75f, 0.125f}}),
            error
        );

        REQUIRE_FALSE(error.has_error());
        REQUIRE(second_asset_handle.is_valid());
        REQUIRE_EQ(second_asset_handle.id, first_asset_handle.id);
        REQUIRE_NE(second_asset_handle.generation, first_asset_handle.generation);
    }

    SUBCASE("AudioSpace - retired playback slots are reused with a new generation") {
        const Lowl::AudioAssetHandle first_asset_handle = audio_space.add_audio(
            make_stereo_audio_data({StereoSample{0.25f, 0.25f}}),
            error
        );

        REQUIRE_FALSE(error.has_error());
        REQUIRE(first_asset_handle.is_valid());

        const Lowl::AudioPlaybackHandle first_playback = audio_space.create_playback(first_asset_handle);
        REQUIRE(first_playback.is_valid());

        audio_space.play(first_playback);
        audio_space.clear_all_audio();

        auto [cleared_result, cleared_frame] = render_one_frame(audio_space);
        REQUIRE_EQ(cleared_result.frames_produced, 0U);
        REQUIRE_EQ(cleared_result.state, Lowl::Audio::AudioSource::RenderState::Finished);
        REQUIRE_EQ(cleared_frame.left, doctest::Approx(0.0f));
        REQUIRE_EQ(cleared_frame.right, doctest::Approx(0.0f));

        const Lowl::AudioAssetHandle second_asset_handle = audio_space.add_audio(
            make_stereo_audio_data({StereoSample{0.75f, -0.25f}}),
            error
        );

        REQUIRE_FALSE(error.has_error());
        REQUIRE(second_asset_handle.is_valid());

        const Lowl::AudioPlaybackHandle second_playback = audio_space.create_playback(second_asset_handle);
        REQUIRE(second_playback.is_valid());
        REQUIRE_EQ(second_playback.id, first_playback.id);
        REQUIRE_NE(second_playback.generation, first_playback.generation);

        audio_space.play(first_playback);

        auto [stale_result, stale_frame] = render_one_frame(audio_space);
        REQUIRE_EQ(stale_result.frames_produced, 0U);
        REQUIRE_EQ(stale_result.state, Lowl::Audio::AudioSource::RenderState::Finished);
        REQUIRE_EQ(stale_frame.left, doctest::Approx(0.0f));
        REQUIRE_EQ(stale_frame.right, doctest::Approx(0.0f));

        audio_space.play(second_playback);

        auto [reused_result, reused_frame] = render_one_frame(audio_space);
        REQUIRE_EQ(reused_result.frames_produced, 1U);
        REQUIRE_EQ(reused_result.state, Lowl::Audio::AudioSource::RenderState::Ok);
        REQUIRE_EQ(reused_frame.left, doctest::Approx(0.75f));
        REQUIRE_EQ(reused_frame.right, doctest::Approx(-0.25f));
    }

    SUBCASE("AudioSpace - retired asset slots are reused with a new generation") {
        const Lowl::AudioAssetHandle first_asset_handle = audio_space.add_audio(
            make_stereo_audio_data({StereoSample{0.125f, 0.25f}}),
            error
        );

        REQUIRE_FALSE(error.has_error());
        REQUIRE(first_asset_handle.is_valid());

        const Lowl::AudioPlaybackHandle first_playback = audio_space.create_playback(first_asset_handle);
        REQUIRE(first_playback.is_valid());

        audio_space.clear_all_audio();

        const Lowl::AudioAssetHandle second_asset_handle = audio_space.add_audio(
            make_stereo_audio_data({StereoSample{0.5f, -0.5f}}),
            error
        );

        REQUIRE_FALSE(error.has_error());
        REQUIRE(second_asset_handle.is_valid());
        REQUIRE_EQ(second_asset_handle.id, first_asset_handle.id);
        REQUIRE_NE(second_asset_handle.generation, first_asset_handle.generation);
        REQUIRE_EQ(audio_space.create_playback(first_asset_handle), Lowl::Audio::AudioSpace::InvalidAudioPlaybackHandle);
        REQUIRE(audio_space.create_playback(second_asset_handle).is_valid());
    }

    SUBCASE("AudioSpace - create_playback fails cleanly at 16-bit exhaustion and recovers after destroy_playback") {
        const Lowl::AudioAssetHandle asset_handle = audio_space.add_audio(
            make_stereo_audio_data({StereoSample{0.125f, -0.25f}}),
            error
        );

        REQUIRE_FALSE(error.has_error());
        REQUIRE(asset_handle.is_valid());

        const size_t max_playbacks = static_cast<size_t>(std::numeric_limits<Lowl::AudioPlaybackId>::max());
        std::vector<Lowl::AudioPlaybackHandle> playback_handles;
        playback_handles.reserve(max_playbacks);

        for (size_t playback_index = 0; playback_index < max_playbacks; playback_index++) {
            const Lowl::AudioPlaybackHandle playback_handle = audio_space.create_playback(asset_handle);
            REQUIRE(playback_handle.is_valid());
            playback_handles.push_back(playback_handle);
        }

        REQUIRE_EQ(audio_space.create_playback(asset_handle), Lowl::Audio::AudioSpace::InvalidAudioPlaybackHandle);

        const Lowl::AudioPlaybackHandle retired_playback = playback_handles.front();
        audio_space.destroy_playback(retired_playback);

        const Lowl::AudioPlaybackHandle replacement_playback = audio_space.create_playback(asset_handle);
        REQUIRE(replacement_playback.is_valid());
        REQUIRE_EQ(replacement_playback.id, retired_playback.id);
        REQUIRE_NE(replacement_playback.generation, retired_playback.generation);
    }

    SUBCASE("AudioSpace - handles are rejected across spaces even when ids and generations match") {
        Lowl::Error other_error;
        Lowl::Audio::AudioSpace other_audio_space(44100.0, Lowl::Audio::ChannelLayout::Stereo);

        const Lowl::AudioAssetHandle first_asset_handle = audio_space.add_audio(
            make_stereo_audio_data({StereoSample{0.125f, 0.25f}}),
            error
        );
        const Lowl::AudioAssetHandle second_asset_handle = other_audio_space.add_audio(
            make_stereo_audio_data({StereoSample{0.5f, -0.5f}}),
            other_error
        );

        REQUIRE_FALSE(error.has_error());
        REQUIRE_FALSE(other_error.has_error());
        REQUIRE(first_asset_handle.is_valid());
        REQUIRE(second_asset_handle.is_valid());
        REQUIRE_EQ(first_asset_handle.id, second_asset_handle.id);
        REQUIRE_EQ(first_asset_handle.generation, second_asset_handle.generation);
        REQUIRE_NE(first_asset_handle.owner_id, second_asset_handle.owner_id);
        REQUIRE_EQ(other_audio_space.create_playback(first_asset_handle),
                   Lowl::Audio::AudioSpace::InvalidAudioPlaybackHandle);

        const Lowl::AudioPlaybackHandle first_playback = audio_space.create_playback(first_asset_handle);
        const Lowl::AudioPlaybackHandle second_playback = other_audio_space.create_playback(second_asset_handle);

        REQUIRE(first_playback.is_valid());
        REQUIRE(second_playback.is_valid());
        REQUIRE_EQ(first_playback.id, second_playback.id);
        REQUIRE_EQ(first_playback.generation, second_playback.generation);
        REQUIRE_NE(first_playback.owner_id, second_playback.owner_id);
        REQUIRE_EQ(other_audio_space.get_frame_count(first_playback), 0U);
        REQUIRE_GT(other_audio_space.get_frame_count(second_playback), 0U);
    }
}
