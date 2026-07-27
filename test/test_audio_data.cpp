#include <doctest/doctest.h>
#include <lowl.h>

#include <atomic>
#include <chrono>
#include <memory>
#include <thread>
#include <vector>

#include "audio/lowl_audio_buffer.h"
#include "audio/source/lowl_audio_published_playback_state.h"
#include "audio/source/lowl_audio_voice.h"

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
            std::move(storage), frame_count, Lowl::Audio::AudioFormat{44100.0, Lowl::Audio::ChannelLayout::Stereo});
    }

    std::unique_ptr<Lowl::Audio::AudioData> make_audio_data(Lowl::Audio::ChannelLayout p_layout,
                                                            const std::vector<Lowl::Sample> &p_interleaved_frames) {
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
        return std::make_unique<Lowl::Audio::AudioData>(
            std::move(storage), frame_count, Lowl::Audio::AudioFormat{44100.0, p_layout});
    }
} // namespace

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

    SUBCASE("AudioVoice - starts stopped until playback is explicitly started") {
        REQUIRE_EQ(audio_voice.get_playback_state(), Lowl::Audio::AudioVoice::PlaybackState::Stopped);
        REQUIRE(audio_voice.is_pause());

        auto [stopped_result, stopped_frame] = render_one_frame(audio_voice);
        REQUIRE_EQ(stopped_result.frames_produced, 0U);
        REQUIRE_EQ(stopped_result.state, Lowl::Audio::AudioSource::RenderState::Starved);
        REQUIRE_EQ(stopped_frame.left, doctest::Approx(0.0f));
        REQUIRE_EQ(stopped_frame.right, doctest::Approx(0.0f));

        audio_voice.restart_playback();

        auto [started_result, started_frame] = render_one_frame(audio_voice);
        REQUIRE_EQ(started_result.frames_produced, 1U);
        REQUIRE_EQ(started_result.state, Lowl::Audio::AudioSource::RenderState::Remove);
        REQUIRE_EQ(started_frame.left, doctest::Approx(0.5f));
        REQUIRE_EQ(started_frame.right, doctest::Approx(0.5f));
    }

    SUBCASE("AudioVoice - Frame") {
        audio_voice.restart_playback();
        auto [result, read] = render_one_frame(audio_voice);
        REQUIRE_EQ(result.frames_produced, 1U);
        REQUIRE_EQ(result.state, Lowl::Audio::AudioSource::RenderState::Remove);
        REQUIRE_EQ(read.left, doctest::Approx(0.5f));
        REQUIRE_EQ(read.right, doctest::Approx(0.5f));
    }

    SUBCASE("AudioVoice - Panning") {
        audio_voice.restart_playback();
        auto [result0, read0] = render_one_frame(audio_voice);
        REQUIRE_EQ(result0.frames_produced, 1U);
        REQUIRE_EQ(result0.state, Lowl::Audio::AudioSource::RenderState::Remove);
        REQUIRE_EQ(read0.left, 0.5);
        REQUIRE_EQ(read0.right, 0.5);

        audio_voice.set_panning(1);
        audio_voice.restart_playback();
        auto [result1, read1] = render_one_frame(audio_voice);
        REQUIRE_EQ(result1.frames_produced, 1U);
        REQUIRE_EQ(result1.state, Lowl::Audio::AudioSource::RenderState::Remove);
        REQUIRE_EQ(read1.left, 0.0);
        REQUIRE_EQ(read1.right, doctest::Approx(0.70711));

        audio_voice.set_panning(-1);
        audio_voice.restart_playback();
        auto [result2, read2] = render_one_frame(audio_voice);
        REQUIRE_EQ(result2.frames_produced, 1U);
        REQUIRE_EQ(result2.state, Lowl::Audio::AudioSource::RenderState::Remove);
        REQUIRE_EQ(read2.left, doctest::Approx(0.70711));
        REQUIRE_EQ(read2.right, 0.0);

        audio_voice.set_panning(0);
        audio_voice.restart_playback();
        auto [result3, read3] = render_one_frame(audio_voice);
        REQUIRE_EQ(result3.frames_produced, 1U);
        REQUIRE_EQ(result3.state, Lowl::Audio::AudioSource::RenderState::Remove);
        REQUIRE_EQ(read3.left, 0.5);
        REQUIRE_EQ(read3.right, 0.5);
    }

    SUBCASE("AudioVoice - multichannel panning only touches front left and right speakers") {
        std::shared_ptr<Lowl::Audio::AudioData> surround_audio = std::move(
            make_audio_data(Lowl::Audio::ChannelLayout::Surround_5_1, {0.5f, 0.5f, 0.25f, 0.125f, 0.75f, -0.75f}));
        Lowl::Audio::AudioVoice surround_voice(surround_audio);
        surround_voice.set_panning(1);
        surround_voice.restart_playback();

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
        mono_voice.restart_playback();

        Lowl::Audio::AudioBuffer buffer(1, mono_voice.get_channel_count());
        Lowl::Audio::AudioBlockView block = buffer.view(1);
        buffer.clear(1);
        Lowl::Audio::AudioSource::RenderResult result = mono_voice.render(block);

        REQUIRE_EQ(result.frames_produced, 1U);
        REQUIRE_EQ(result.state, Lowl::Audio::AudioSource::RenderState::Remove);
        REQUIRE_EQ(block.channel(0)[0], doctest::Approx(0.5f));
    }

    SUBCASE("AudioVoice - render rejects mismatched channel block") {
        audio_voice.restart_playback();
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
            std::move(storage), 3, Lowl::Audio::AudioFormat{10.0, Lowl::Audio::ChannelLayout::Stereo});

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
            std::move(storage), 3, Lowl::Audio::AudioFormat{10.0, Lowl::Audio::ChannelLayout::Stereo});

        std::unique_ptr<Lowl::Audio::AudioData> slice = sliced_source->create_slice(-1.0, 0.2);
        REQUIRE(slice != nullptr);
        REQUIRE_EQ(slice->get_frame_count(), 2U);
        REQUIRE_EQ(slice->get_channel_data(0)[0], doctest::Approx(0.10f));
        REQUIRE_EQ(slice->get_channel_data(0)[1], doctest::Approx(0.30f));
        REQUIRE_EQ(slice->get_channel_data(1)[0], doctest::Approx(0.20f));
        REQUIRE_EQ(slice->get_channel_data(1)[1], doctest::Approx(0.40f));
    }

    SUBCASE("AudioData - create_slice supports zero-length slices") {
        std::unique_ptr<Lowl::Audio::AudioData> sliced_source = make_stereo_audio_data({
            StereoSample{0.10f, 0.20f},
            StereoSample{0.30f, 0.40f},
        });

        std::unique_ptr<Lowl::Audio::AudioData> slice = sliced_source->create_slice(0.1, 0.1);
        REQUIRE(slice != nullptr);
        REQUIRE_EQ(slice->get_frame_count(), 0U);
        REQUIRE_EQ(slice->get_channel_count(), 2U);
    }

    SUBCASE("AudioData - create_slice preserves mono channel data") {
        std::unique_ptr<Lowl::Audio::AudioData> mono_audio =
            std::move(make_audio_data(Lowl::Audio::ChannelLayout::Mono, {0.1f, 0.2f, 0.3f}));

        std::unique_ptr<Lowl::Audio::AudioData> slice = mono_audio->create_slice(1.0 / 44100.0, 3.0 / 44100.0);
        REQUIRE(slice != nullptr);
        REQUIRE_EQ(slice->get_channel_count(), 1U);
        REQUIRE_EQ(slice->get_frame_count(), 2U);
        REQUIRE_EQ(slice->get_channel_data(0)[0], doctest::Approx(0.2f));
        REQUIRE_EQ(slice->get_channel_data(0)[1], doctest::Approx(0.3f));
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

    SUBCASE("AudioVoice - stop_playback never publishes stopped with a stale position") {
        std::shared_ptr<Lowl::Audio::AudioData> long_audio = std::move(make_stereo_audio_data({
            StereoSample{0.10f, 0.20f},
            StereoSample{0.30f, 0.40f},
            StereoSample{0.50f, 0.60f},
            StereoSample{0.70f, 0.80f},
        }));
        Lowl::Audio::AudioVoice voice(long_audio);
        voice.restart_playback();

        std::atomic<bool> start{false};
        std::atomic<bool> invalid_state_seen{false};

        std::thread writer([&]() {
            while (!start.load(std::memory_order_acquire)) {
            }
            for (int index = 0; index < 250000 && !invalid_state_seen.load(std::memory_order_relaxed); index++) {
                voice.seek_frame(2);
                voice.stop_playback();
                voice.restart_playback();
            }
        });

        std::thread reader([&]() {
            while (!start.load(std::memory_order_acquire)) {
            }
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
            while (std::chrono::steady_clock::now() < deadline && !invalid_state_seen.load(std::memory_order_relaxed)) {
                const Lowl::Audio::AudioVoice::PlaybackSnapshot snapshot = voice.get_playback_snapshot();
                if (snapshot.playback_state == Lowl::Audio::AudioVoice::PlaybackState::Stopped &&
                    snapshot.frame_position != 0U) {
                    invalid_state_seen.store(true, std::memory_order_relaxed);
                    return;
                }
                std::this_thread::yield();
            }
        });

        start.store(true, std::memory_order_release);
        writer.join();
        reader.join();

        REQUIRE_FALSE(invalid_state_seen.load(std::memory_order_relaxed));
    }

    SUBCASE("AudioVoice - published snapshots stay coherent above the 32-bit frame boundary") {
        using PlaybackState = Lowl::Audio::AudioVoice::PlaybackState;
        using PlaybackSnapshot = Lowl::Audio::AudioVoice::PlaybackSnapshot;
        using PublishedState =
            Lowl::Audio::Detail::PublishedPlaybackState<PlaybackSnapshot, PlaybackState, Lowl::Sample>;

        if constexpr (sizeof(size_t) > sizeof(uint32_t)) {
            const size_t high_position_a = static_cast<size_t>((uint64_t{1} << 32U) + 17U);
            const size_t high_position_b = static_cast<size_t>((uint64_t{1} << 32U) + 29U);
            const PlaybackSnapshot snapshot_a{high_position_a, PlaybackState::Playing};
            const PlaybackSnapshot snapshot_b{high_position_b, PlaybackState::Paused};

            PublishedState published_state;
            published_state.store(snapshot_a);

            PlaybackSnapshot round_trip = published_state.load();
            REQUIRE_EQ(round_trip.frame_position, high_position_a);
            REQUIRE_EQ(round_trip.playback_state, PlaybackState::Playing);

            std::atomic<bool> start{false};
            std::atomic<bool> writer_done{false};
            std::atomic<bool> invalid_snapshot_seen{false};

            std::thread writer([&]() {
                while (!start.load(std::memory_order_acquire)) {
                }
                for (size_t iteration = 0; iteration < 250000; iteration++) {
                    published_state.store((iteration & 1U) == 0U ? snapshot_b : snapshot_a);
                }
                writer_done.store(true, std::memory_order_release);
            });

            std::thread reader([&]() {
                while (!start.load(std::memory_order_acquire)) {
                }
                while (!writer_done.load(std::memory_order_acquire) &&
                       !invalid_snapshot_seen.load(std::memory_order_relaxed)) {
                    const PlaybackSnapshot snapshot = published_state.load();
                    const bool matches_a = snapshot.frame_position == snapshot_a.frame_position &&
                                           snapshot.playback_state == snapshot_a.playback_state;
                    const bool matches_b = snapshot.frame_position == snapshot_b.frame_position &&
                                           snapshot.playback_state == snapshot_b.playback_state;
                    if (!matches_a && !matches_b) {
                        invalid_snapshot_seen.store(true, std::memory_order_relaxed);
                    }
                }
            });

            start.store(true, std::memory_order_release);
            writer.join();
            reader.join();

            REQUIRE_FALSE(invalid_snapshot_seen.load(std::memory_order_relaxed));
        } else {
            MESSAGE("Above-32-bit snapshot coverage requires a 64-bit size_t target.");
        }
    }
}
