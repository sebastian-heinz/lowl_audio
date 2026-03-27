#include <doctest/doctest.h>

#include <lowl.h>
#include "audio/lowl_audio_buffer.h"

#include <iostream>
#include <chrono>
#include <memory>
#include <thread>
#include <vector>

namespace {
    struct StereoSample {
        Lowl::Sample left;
        Lowl::Sample right;
    };

    std::unique_ptr<Lowl::Audio::AudioData>
    make_audio_data(Lowl::Audio::ChannelLayout p_channel_layout, const std::vector<Lowl::Sample> &p_interleaved_frames) {
        const uint8_t channel_count = p_channel_layout.channel_count;
        const size_t frame_count = channel_count == 0 ? 0 : p_interleaved_frames.size() / channel_count;
        std::unique_ptr<Lowl::Sample[]> storage;
        if (frame_count > 0 && channel_count > 0) {
            storage = std::make_unique<Lowl::Sample[]>(frame_count * channel_count);
            for (size_t frame_index = 0; frame_index < frame_count; frame_index++) {
                for (size_t channel_index = 0; channel_index < channel_count; channel_index++) {
                    storage[channel_index * frame_count + frame_index] =
                        p_interleaved_frames[frame_index * channel_count + channel_index];
                }
            }
        }
        return std::make_unique<Lowl::Audio::AudioData>(std::move(storage), frame_count, 44100.0, p_channel_layout);
    }

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

    Lowl::AudioPlaybackHandle add_asset_and_create_playback(Lowl::Audio::AudioSpace &p_audio_space,
                                                            std::unique_ptr<Lowl::Audio::AudioData> p_audio_data,
                                                            Lowl::Error &p_error) {
        const Lowl::AudioAssetHandle asset_handle = p_audio_space.add_audio(std::move(p_audio_data), p_error);
        if (p_error.has_error() || !asset_handle.is_valid()) {
            return Lowl::Audio::AudioSpace::InvalidAudioPlaybackHandle;
        }
        return p_audio_space.create_playback(asset_handle);
    }

    Lowl::uint16_l register_mixer_owner(Lowl::Audio::AudioMixer &p_mixer) {
        const Lowl::uint16_l owner_id = p_mixer.register_ack_owner();
        REQUIRE_NE(owner_id, 0);
        return owner_id;
    }

    Lowl::AudioMixerHandle allocate_mixer_handle(Lowl::Audio::AudioMixer &p_mixer, const Lowl::uint16_l p_owner_id) {
        const Lowl::AudioMixerHandle handle = p_mixer.allocate_handle(p_owner_id);
        REQUIRE(handle.is_valid());
        return handle;
    }

    void expect_mixer_ack(Lowl::Audio::AudioMixer &p_mixer,
                          const Lowl::uint16_l p_owner_id,
                          const Lowl::AudioMixerHandle p_handle,
                          const Lowl::Audio::AudioMixerAck::Type p_type) {
        Lowl::Audio::AudioMixerAck ack{};
        REQUIRE(p_mixer.try_dequeue_ack(p_owner_id, ack));
        REQUIRE(ack.handle == p_handle);
        REQUIRE_EQ(ack.type, p_type);
    }

    class MixerDetachProbe final : public Lowl::Audio::AudioSource {
    public:
        explicit MixerDetachProbe(Lowl::Audio::ChannelLayout p_channel_layout)
            : AudioSource(44100.0, p_channel_layout) {
        }

        RenderResult render(Lowl::Audio::AudioBlockView) override {
            return {0, RenderState::Starved};
        }

        Lowl::size_l get_frames_remaining() const override {
            return 0;
        }

        Lowl::size_l get_frame_position() const override {
            return 0;
        }

        Lowl::size_l get_frame_count() const override {
            return 0;
        }

        void on_removed_from_mixer() override {
            detached = true;
        }

        bool detached = false;
    };

    class ConstantMixerSource final : public Lowl::Audio::AudioSource {
    public:
        explicit ConstantMixerSource(const StereoSample p_sample)
            : AudioSource(44100.0, Lowl::Audio::ChannelLayout::Stereo),
              sample(p_sample) {
        }

        RenderResult render(Lowl::Audio::AudioBlockView p_block) override {
            if (p_block.channel_count != 2 || p_block.frame_count == 0) {
                return {0, RenderState::Starved};
            }
            p_block.channel(0)[0] = sample.left;
            p_block.channel(1)[0] = sample.right;
            return {1, RenderState::Ok};
        }

        Lowl::size_l get_frames_remaining() const override {
            return 1;
        }

        Lowl::size_l get_frame_position() const override {
            return 0;
        }

        Lowl::size_l get_frame_count() const override {
            return 1;
        }

    private:
        StereoSample sample{};
    };
}

TEST_CASE("AudioStream") {
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

    std::shared_ptr<Lowl::Audio::AudioStream> audio_stream
            = std::make_unique<Lowl::Audio::AudioStream>(44100.0, Lowl::Audio::ChannelLayout::Stereo);

    SUBCASE("AudioStream - Frame") {
        const Lowl::Sample samples[] = {0.5f, 0.5f};
        REQUIRE_EQ(audio_stream->write_interleaved(samples, 1), 1U);
        auto [result0, read0] = render_one_frame(*audio_stream);
        REQUIRE_EQ(result0.frames_produced, 1U);
        REQUIRE_EQ(result0.state, Lowl::Audio::AudioSource::RenderState::Ok);
        REQUIRE_EQ(read0.left, doctest::Approx(0.5f));
        REQUIRE_EQ(read0.right, doctest::Approx(0.5f));

        auto [result1, read1] = render_one_frame(*audio_stream);
        REQUIRE_EQ(result1.frames_produced, 0U);
        REQUIRE_EQ(result1.state, Lowl::Audio::AudioSource::RenderState::Starved);
        REQUIRE_EQ(read1.left, doctest::Approx(0.0f));
        REQUIRE_EQ(read1.right, doctest::Approx(0.0f));
    }

    SUBCASE("AudioStream - Panning") {
        const Lowl::Sample samples[] = {0.5f, 0.5f};
        REQUIRE_EQ(audio_stream->write_interleaved(samples, 1), 1U);
        auto [result0, read0] = render_one_frame(*audio_stream);
        REQUIRE_EQ(result0.frames_produced, 1U);
        REQUIRE_EQ(result0.state, Lowl::Audio::AudioSource::RenderState::Ok);
        REQUIRE_EQ(read0.left, 0.5);
        REQUIRE_EQ(read0.right, 0.5);

        audio_stream->set_panning(1);
        REQUIRE_EQ(audio_stream->write_interleaved(samples, 1), 1U);
        auto [result1, read1] = render_one_frame(*audio_stream);
        REQUIRE_EQ(result1.frames_produced, 1U);
        REQUIRE_EQ(result1.state, Lowl::Audio::AudioSource::RenderState::Ok);
        REQUIRE_EQ(read1.left, 0.0);
        REQUIRE_EQ(read1.right, doctest::Approx(0.70711));

        audio_stream->set_panning(-1);
        REQUIRE_EQ(audio_stream->write_interleaved(samples, 1), 1U);
        auto [result2, read2] = render_one_frame(*audio_stream);
        REQUIRE_EQ(result2.frames_produced, 1U);
        REQUIRE_EQ(result2.state, Lowl::Audio::AudioSource::RenderState::Ok);
        REQUIRE_EQ(read2.left, doctest::Approx(0.70711));
        REQUIRE_EQ(read2.right, 0.0);

        audio_stream->set_panning(0);
        REQUIRE_EQ(audio_stream->write_interleaved(samples, 1), 1U);
        auto [result3, read3] = render_one_frame(*audio_stream);
        REQUIRE_EQ(result3.frames_produced, 1U);
        REQUIRE_EQ(result3.state, Lowl::Audio::AudioSource::RenderState::Ok);
        REQUIRE_EQ(read3.left, 0.5);
        REQUIRE_EQ(read3.right, 0.5);
    }

    SUBCASE("AudioStream - Planar write wraps around ring") {
        Lowl::Audio::AudioStream small_stream(44100.0, Lowl::Audio::ChannelLayout::Stereo, 3);
        const Lowl::Sample left_a[] = {0.1f, 0.2f};
        const Lowl::Sample right_a[] = {-0.1f, -0.2f};
        const std::vector<const Lowl::Sample *> first_block{left_a, right_a};
        REQUIRE_EQ(small_stream.write_planar(first_block, 2), 2U);

        auto [result0, read0] = render_one_frame(small_stream);
        REQUIRE_EQ(result0.frames_produced, 1U);
        REQUIRE_EQ(read0.left, doctest::Approx(0.1f));
        REQUIRE_EQ(read0.right, doctest::Approx(-0.1f));

        const Lowl::Sample left_b[] = {0.3f, 0.4f};
        const Lowl::Sample right_b[] = {-0.3f, -0.4f};
        const std::vector<const Lowl::Sample *> second_block{left_b, right_b};
        REQUIRE_EQ(small_stream.write_planar(second_block, 2), 2U);

        auto [result1, read1] = render_one_frame(small_stream);
        REQUIRE_EQ(result1.frames_produced, 1U);
        REQUIRE_EQ(read1.left, doctest::Approx(0.2f));
        REQUIRE_EQ(read1.right, doctest::Approx(-0.2f));

        auto [result2, read2] = render_one_frame(small_stream);
        REQUIRE_EQ(result2.frames_produced, 1U);
        REQUIRE_EQ(read2.left, doctest::Approx(0.3f));
        REQUIRE_EQ(read2.right, doctest::Approx(-0.3f));

        auto [result3, read3] = render_one_frame(small_stream);
        REQUIRE_EQ(result3.frames_produced, 1U);
        REQUIRE_EQ(read3.left, doctest::Approx(0.4f));
        REQUIRE_EQ(read3.right, doctest::Approx(-0.4f));
    }

    SUBCASE("AudioStream - Interleaved write wraps around ring") {
        Lowl::Audio::AudioStream small_stream(44100.0, Lowl::Audio::ChannelLayout::Stereo, 3);
        const Lowl::Sample first_block[] = {0.1f, -0.1f, 0.2f, -0.2f};
        REQUIRE_EQ(small_stream.write_interleaved(first_block, 2), 2U);

        auto [result0, read0] = render_one_frame(small_stream);
        REQUIRE_EQ(result0.frames_produced, 1U);
        REQUIRE_EQ(read0.left, doctest::Approx(0.1f));
        REQUIRE_EQ(read0.right, doctest::Approx(-0.1f));

        const Lowl::Sample second_block[] = {0.3f, -0.3f, 0.4f, -0.4f};
        REQUIRE_EQ(small_stream.write_interleaved(second_block, 2), 2U);

        auto [result1, read1] = render_one_frame(small_stream);
        REQUIRE_EQ(result1.frames_produced, 1U);
        REQUIRE_EQ(read1.left, doctest::Approx(0.2f));
        REQUIRE_EQ(read1.right, doctest::Approx(-0.2f));

        auto [result2, read2] = render_one_frame(small_stream);
        REQUIRE_EQ(result2.frames_produced, 1U);
        REQUIRE_EQ(read2.left, doctest::Approx(0.3f));
        REQUIRE_EQ(read2.right, doctest::Approx(-0.3f));

        auto [result3, read3] = render_one_frame(small_stream);
        REQUIRE_EQ(result3.frames_produced, 1U);
        REQUIRE_EQ(read3.left, doctest::Approx(0.4f));
        REQUIRE_EQ(read3.right, doctest::Approx(-0.4f));
    }

    SUBCASE("AudioStream - writes stop at capacity and resume after reads") {
        Lowl::Audio::AudioStream small_stream(44100.0, Lowl::Audio::ChannelLayout::Stereo, 2);
        const Lowl::Sample first_block[] = {0.1f, -0.1f, 0.2f, -0.2f, 0.3f, -0.3f};
        REQUIRE_EQ(small_stream.write_interleaved(first_block, 3), 2U);

        auto [result0, read0] = render_one_frame(small_stream);
        REQUIRE_EQ(result0.frames_produced, 1U);
        REQUIRE_EQ(read0.left, doctest::Approx(0.1f));
        REQUIRE_EQ(read0.right, doctest::Approx(-0.1f));

        const Lowl::Sample second_block[] = {0.4f, -0.4f};
        REQUIRE_EQ(small_stream.write_interleaved(second_block, 1), 1U);

        auto [result1, read1] = render_one_frame(small_stream);
        REQUIRE_EQ(result1.frames_produced, 1U);
        REQUIRE_EQ(read1.left, doctest::Approx(0.2f));
        REQUIRE_EQ(read1.right, doctest::Approx(-0.2f));

        auto [result2, read2] = render_one_frame(small_stream);
        REQUIRE_EQ(result2.frames_produced, 1U);
        REQUIRE_EQ(read2.left, doctest::Approx(0.4f));
        REQUIRE_EQ(read2.right, doctest::Approx(-0.4f));
    }

    SUBCASE("AudioStream - concurrent single producer and single consumer preserve frame order") {
        constexpr size_t total_frames = 64;
        Lowl::Audio::AudioStream stream(44100.0, Lowl::Audio::ChannelLayout::Stereo, 4);
        std::vector<StereoSample> expected_frames(total_frames);
        for (size_t frame_index = 0; frame_index < total_frames; frame_index++) {
            expected_frames[frame_index] = {
                static_cast<Lowl::Sample>(frame_index) / 100.0f,
                static_cast<Lowl::Sample>(-static_cast<int>(frame_index)) / 100.0f
            };
        }

        std::thread writer([&]() {
            for (size_t frame_index = 0; frame_index < total_frames;) {
                const Lowl::Sample frame[] = {
                    expected_frames[frame_index].left,
                    expected_frames[frame_index].right,
                };
                if (stream.write_interleaved(frame, 1) == 1U) {
                    frame_index++;
                } else {
                    std::this_thread::yield();
                }
            }
        });

        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
        for (size_t frame_index = 0; frame_index < total_frames; ) {
            REQUIRE(std::chrono::steady_clock::now() < deadline);
            auto [result, frame] = render_one_frame(stream);
            if (result.frames_produced == 0U) {
                std::this_thread::yield();
                continue;
            }
            REQUIRE_EQ(frame.left, doctest::Approx(expected_frames[frame_index].left));
            REQUIRE_EQ(frame.right, doctest::Approx(expected_frames[frame_index].right));
            frame_index++;
        }

        writer.join();
    }

    SUBCASE("AudioStream - render rejects mismatched channel block") {
        Lowl::Audio::AudioStream mono_stream(44100.0, Lowl::Audio::ChannelLayout::Mono, 3);
        const Lowl::Sample samples[] = {0.5f};
        REQUIRE_EQ(mono_stream.write_interleaved(samples, 1), 1U);

        Lowl::Audio::AudioBuffer stereo_buffer(1, 2);
        Lowl::Audio::AudioBlockView stereo_block = stereo_buffer.view(1);
        stereo_buffer.clear(1);

        Lowl::Audio::AudioSource::RenderResult result = mono_stream.render(stereo_block);
        REQUIRE_EQ(result.frames_produced, 0U);
        REQUIRE_EQ(result.state, Lowl::Audio::AudioSource::RenderState::Error);
        REQUIRE_EQ(stereo_block.channel(0)[0], doctest::Approx(0.0f));
        REQUIRE_EQ(stereo_block.channel(1)[0], doctest::Approx(0.0f));
    }

    SUBCASE("AudioMixer - render rejects mismatched channel block") {
        Lowl::Audio::AudioMixer mixer(44100.0, Lowl::Audio::ChannelLayout::Stereo);

        Lowl::Audio::AudioBuffer mono_buffer(1, 1);
        Lowl::Audio::AudioBlockView mono_block = mono_buffer.view(1);
        mono_buffer.clear(1);

        Lowl::Audio::AudioSource::RenderResult result = mixer.render(mono_block);
        REQUIRE_EQ(result.frames_produced, 0U);
        REQUIRE_EQ(result.state, Lowl::Audio::AudioSource::RenderState::Error);
        REQUIRE_EQ(mono_block.channel(0)[0], doctest::Approx(0.0f));
    }

    SUBCASE("AudioMixer - mix rejects mismatched channel source") {
        Lowl::Audio::AudioMixer mixer(44100.0, Lowl::Audio::ChannelLayout::Stereo);
        MixerDetachProbe mono_probe(Lowl::Audio::ChannelLayout::Mono);
        const Lowl::uint16_l owner_id = register_mixer_owner(mixer);
        const Lowl::AudioMixerHandle mono_handle = allocate_mixer_handle(mixer, owner_id);

        mixer.mix(mono_handle, &mono_probe);

        REQUIRE_FALSE(mono_probe.detached);
        expect_mixer_ack(mixer, owner_id, mono_handle, Lowl::Audio::AudioMixerAck::Type::Rejected);
        mixer.release_handle(mono_handle);
    }

    SUBCASE("AudioMixer - remove reuses a freed slot") {
        Lowl::Audio::AudioMixer mixer(44100.0, Lowl::Audio::ChannelLayout::Stereo);
        ConstantMixerSource source_a({0.25f, 0.25f});
        ConstantMixerSource source_b({0.50f, 0.50f});
        ConstantMixerSource source_c({0.75f, 0.75f});
        const Lowl::uint16_l owner_id = register_mixer_owner(mixer);
        const Lowl::AudioMixerHandle handle_a = allocate_mixer_handle(mixer, owner_id);
        const Lowl::AudioMixerHandle handle_b = allocate_mixer_handle(mixer, owner_id);
        const Lowl::AudioMixerHandle handle_c = allocate_mixer_handle(mixer, owner_id);

        Lowl::Audio::AudioBuffer buffer(1, 2);
        Lowl::Audio::AudioBlockView block = buffer.view(1);

        mixer.mix(handle_a, &source_a);
        mixer.mix(handle_b, &source_b);

        buffer.clear(1);
        Lowl::Audio::AudioSource::RenderResult first_result = mixer.render(block);
        REQUIRE_EQ(first_result.frames_produced, 1U);
        REQUIRE_EQ(first_result.state, Lowl::Audio::AudioSource::RenderState::Ok);
        REQUIRE_EQ(block.channel(0)[0], doctest::Approx(0.75f));
        REQUIRE_EQ(block.channel(1)[0], doctest::Approx(0.75f));

        mixer.remove(handle_b, true);
        mixer.mix(handle_c, &source_c);

        buffer.clear(1);
        Lowl::Audio::AudioSource::RenderResult second_result = mixer.render(block);
        REQUIRE_EQ(second_result.frames_produced, 1U);
        REQUIRE_EQ(second_result.state, Lowl::Audio::AudioSource::RenderState::Ok);
        REQUIRE_EQ(block.channel(0)[0], doctest::Approx(1.0f));
        REQUIRE_EQ(block.channel(1)[0], doctest::Approx(1.0f));
        expect_mixer_ack(mixer, owner_id, handle_b, Lowl::Audio::AudioMixerAck::Type::Removed);
        mixer.release_handle(handle_b);
        const Lowl::AudioMixerHandle recycled_handle = allocate_mixer_handle(mixer, owner_id);
        REQUIRE_EQ(recycled_handle.playback_id, handle_b.playback_id);
        REQUIRE_NE(recycled_handle.generation, handle_b.generation);
        mixer.release_handle(recycled_handle);
    }

    SUBCASE("AudioMixer - handle cannot be rebound to a different source") {
        Lowl::Audio::AudioMixer mixer(44100.0, Lowl::Audio::ChannelLayout::Stereo);
        ConstantMixerSource source_a({0.25f, 0.25f});
        ConstantMixerSource source_b({0.50f, 0.50f});
        const Lowl::uint16_l owner_id = register_mixer_owner(mixer);
        const Lowl::AudioMixerHandle handle = allocate_mixer_handle(mixer, owner_id);

        Lowl::Audio::AudioBuffer buffer(1, 2);
        Lowl::Audio::AudioBlockView block = buffer.view(1);

        mixer.mix(handle, &source_a);
        buffer.clear(1);
        Lowl::Audio::AudioSource::RenderResult first_result = mixer.render(block);
        REQUIRE_EQ(first_result.frames_produced, 1U);
        REQUIRE_EQ(block.channel(0)[0], doctest::Approx(0.25f));
        REQUIRE_EQ(block.channel(1)[0], doctest::Approx(0.25f));

        mixer.mix(handle, &source_b);
        expect_mixer_ack(mixer, owner_id, handle, Lowl::Audio::AudioMixerAck::Type::Rejected);

        buffer.clear(1);
        Lowl::Audio::AudioSource::RenderResult second_result = mixer.render(block);
        REQUIRE_EQ(second_result.frames_produced, 1U);
        REQUIRE_EQ(block.channel(0)[0], doctest::Approx(0.25f));
        REQUIRE_EQ(block.channel(1)[0], doctest::Approx(0.25f));
    }

    SUBCASE("AudioMixer - duplicate mix events for the same handle and source are idempotent") {
        Lowl::Audio::AudioMixer mixer(44100.0, Lowl::Audio::ChannelLayout::Stereo);
        ConstantMixerSource source({0.25f, 0.25f});
        const Lowl::uint16_l owner_id = register_mixer_owner(mixer);
        const Lowl::AudioMixerHandle handle = allocate_mixer_handle(mixer, owner_id);

        mixer.mix(handle, &source);
        mixer.mix(handle, &source);

        Lowl::Audio::AudioBuffer buffer(1, 2);
        Lowl::Audio::AudioBlockView block = buffer.view(1);
        buffer.clear(1);

        Lowl::Audio::AudioSource::RenderResult result = mixer.render(block);
        REQUIRE_EQ(result.frames_produced, 1U);
        REQUIRE_EQ(block.channel(0)[0], doctest::Approx(0.25f));
        REQUIRE_EQ(block.channel(1)[0], doctest::Approx(0.25f));

        Lowl::Audio::AudioMixerAck ack{};
        REQUIRE_FALSE(mixer.try_dequeue_ack(owner_id, ack));
    }

    SUBCASE("AudioMixer - released handles are rejected") {
        Lowl::Audio::AudioMixer mixer(44100.0, Lowl::Audio::ChannelLayout::Stereo);
        ConstantMixerSource source({0.25f, 0.25f});
        const Lowl::uint16_l owner_id = register_mixer_owner(mixer);
        const Lowl::AudioMixerHandle handle = allocate_mixer_handle(mixer, owner_id);

        mixer.release_handle(handle);
        mixer.mix(handle, &source);

        expect_mixer_ack(mixer, owner_id, handle, Lowl::Audio::AudioMixerAck::Type::Rejected);
    }

    SUBCASE("AudioMixer - ack owner lifecycle invalidates old handles and allows reuse") {
        Lowl::Audio::AudioMixer mixer(44100.0, Lowl::Audio::ChannelLayout::Stereo);
        const Lowl::uint16_l owner_id = register_mixer_owner(mixer);
        const Lowl::AudioMixerHandle handle = allocate_mixer_handle(mixer, owner_id);

        mixer.unregister_ack_owner(owner_id);

        Lowl::Audio::AudioMixerAck ack{};
        REQUIRE_FALSE(mixer.try_dequeue_ack(owner_id, ack));
        REQUIRE_FALSE(mixer.allocate_handle(owner_id).is_valid());

        const Lowl::uint16_l reused_owner_id = register_mixer_owner(mixer);
        REQUIRE_NE(reused_owner_id, 0);
        REQUIRE(allocate_mixer_handle(mixer, reused_owner_id).is_valid());
    }

    SUBCASE("AudioMixer - rejects sources beyond the active source limit") {
        Lowl::Audio::AudioMixer mixer(44100.0, Lowl::Audio::ChannelLayout::Stereo);
        const Lowl::uint16_l owner_id = register_mixer_owner(mixer);

        std::vector<Lowl::AudioMixerHandle> handles;
        std::vector<std::unique_ptr<ConstantMixerSource>> sources;
        handles.reserve(1025);
        sources.reserve(1025);

        for (size_t index = 0; index < 1025; index++) {
            handles.push_back(allocate_mixer_handle(mixer, owner_id));
            sources.push_back(std::make_unique<ConstantMixerSource>(StereoSample{1.0f, 1.0f}));
            mixer.mix(handles.back(), sources.back().get());
        }

        Lowl::Audio::AudioBuffer buffer(1, 2);
        Lowl::Audio::AudioBlockView block = buffer.view(1);
        buffer.clear(1);

        Lowl::Audio::AudioSource::RenderResult result = mixer.render(block);
        REQUIRE_EQ(result.frames_produced, 1U);
        REQUIRE_EQ(result.state, Lowl::Audio::AudioSource::RenderState::Ok);
        REQUIRE_EQ(block.channel(0)[0], doctest::Approx(1024.0f));
        REQUIRE_EQ(block.channel(1)[0], doctest::Approx(1024.0f));

        int rejected_count = 0;
        Lowl::Audio::AudioMixerAck ack{};
        while (mixer.try_dequeue_ack(owner_id, ack)) {
            if (ack.type == Lowl::Audio::AudioMixerAck::Type::Rejected) {
                rejected_count++;
            }
        }
        REQUIRE_EQ(rejected_count, 1);
    }

    SUBCASE("AudioMixer - render chunks blocks larger than scratch capacity") {
        Lowl::Audio::AudioMixer mixer(44100.0, Lowl::Audio::ChannelLayout::Stereo, 2);
        Lowl::Audio::AudioStream stereo_stream(44100.0, Lowl::Audio::ChannelLayout::Stereo, 8);
        const Lowl::uint16_l owner_id = register_mixer_owner(mixer);
        const Lowl::AudioMixerHandle stream_handle = allocate_mixer_handle(mixer, owner_id);
        const Lowl::Sample samples[] = {
            0.1f, -0.1f,
            0.2f, -0.2f,
            0.3f, -0.3f,
            0.4f, -0.4f,
            0.5f, -0.5f,
        };

        REQUIRE_EQ(stereo_stream.write_interleaved(samples, 5), 5U);
        mixer.mix(stream_handle, &stereo_stream);

        Lowl::Audio::AudioBuffer buffer(5, 2);
        Lowl::Audio::AudioBlockView block = buffer.view(5);
        buffer.clear(5);

        Lowl::Audio::AudioSource::RenderResult result = mixer.render(block);
        REQUIRE_EQ(result.frames_produced, 5U);
        REQUIRE_EQ(result.state, Lowl::Audio::AudioSource::RenderState::Ok);
        REQUIRE_EQ(block.channel(0)[0], doctest::Approx(0.1f));
        REQUIRE_EQ(block.channel(1)[0], doctest::Approx(-0.1f));
        REQUIRE_EQ(block.channel(0)[1], doctest::Approx(0.2f));
        REQUIRE_EQ(block.channel(1)[1], doctest::Approx(-0.2f));
        REQUIRE_EQ(block.channel(0)[2], doctest::Approx(0.3f));
        REQUIRE_EQ(block.channel(1)[2], doctest::Approx(-0.3f));
        REQUIRE_EQ(block.channel(0)[3], doctest::Approx(0.4f));
        REQUIRE_EQ(block.channel(1)[3], doctest::Approx(-0.4f));
        REQUIRE_EQ(block.channel(0)[4], doctest::Approx(0.5f));
        REQUIRE_EQ(block.channel(1)[4], doctest::Approx(-0.5f));
    }

    SUBCASE("AudioMixer - nested mixers sum multichannel AudioSpace and AudioStream inputs correctly") {
        constexpr Lowl::Audio::ChannelLayout channel = Lowl::Audio::ChannelLayout::Surround_5_1;
        constexpr uint32_t frame_count = 10;
        constexpr uint8_t channel_count = 6;

        auto make_interleaved_frames = [](int p_base, int p_channel_scale, int p_frame_scale) {
            std::vector<Lowl::Sample> frames(frame_count * channel_count);
            for (uint32_t frame_index = 0; frame_index < frame_count; frame_index++) {
                for (uint8_t channel_index = 0; channel_index < channel_count; channel_index++) {
                    const int value = p_base + p_channel_scale * static_cast<int>(channel_index + 1) +
                                      p_frame_scale * static_cast<int>(frame_index);
                    frames[frame_index * channel_count + channel_index] = static_cast<Lowl::Sample>(value) / 1000.0f;
                }
            }
            return frames;
        };

        Lowl::Error error;
        Lowl::Audio::AudioSpace audio_space(44100.0, channel);
        Lowl::Audio::AudioMixer mixer_a(44100.0, channel, 3);
        Lowl::Audio::AudioMixer mixer_b(44100.0, channel);
        Lowl::Audio::AudioMixer mixer_c(44100.0, channel);
        Lowl::Audio::AudioStream stream_a(44100.0, channel, 16);
        Lowl::Audio::AudioStream stream_b0(44100.0, channel, 16);
        Lowl::Audio::AudioStream stream_b1(44100.0, channel, 16);
        const Lowl::uint16_l owner_a = register_mixer_owner(mixer_a);
        const Lowl::uint16_l owner_b = register_mixer_owner(mixer_b);
        const Lowl::uint16_l owner_c = register_mixer_owner(mixer_c);
        const Lowl::AudioMixerHandle audio_space_handle = allocate_mixer_handle(mixer_a, owner_a);
        const Lowl::AudioMixerHandle stream_a_handle = allocate_mixer_handle(mixer_a, owner_a);
        const Lowl::AudioMixerHandle stream_b0_handle = allocate_mixer_handle(mixer_b, owner_b);
        const Lowl::AudioMixerHandle stream_b1_handle = allocate_mixer_handle(mixer_b, owner_b);
        const Lowl::AudioMixerHandle mixer_a_handle = allocate_mixer_handle(mixer_c, owner_c);
        const Lowl::AudioMixerHandle mixer_b_handle = allocate_mixer_handle(mixer_c, owner_c);

        const std::vector<Lowl::Sample> space_frames = make_interleaved_frames(100, 10, 1);
        const std::vector<Lowl::Sample> stream_a_frames = make_interleaved_frames(20, 4, 2);
        const std::vector<Lowl::Sample> stream_b0_frames = make_interleaved_frames(-30, 2, 1);
        const std::vector<Lowl::Sample> stream_b1_frames = make_interleaved_frames(5, -1, 3);

        const Lowl::AudioPlaybackHandle playback_handle = add_asset_and_create_playback(
            audio_space,
            make_audio_data(channel, space_frames),
            error
        );

        REQUIRE_FALSE(error.has_error());
        REQUIRE(playback_handle.is_valid());

        REQUIRE_EQ(stream_a.write_interleaved(stream_a_frames.data(), frame_count), frame_count);
        REQUIRE_EQ(stream_b0.write_interleaved(stream_b0_frames.data(), frame_count), frame_count);
        REQUIRE_EQ(stream_b1.write_interleaved(stream_b1_frames.data(), frame_count), frame_count);

        audio_space.play(playback_handle);

        mixer_a.mix(audio_space_handle, &audio_space);
        mixer_a.mix(stream_a_handle, &stream_a);
        mixer_b.mix(stream_b0_handle, &stream_b0);
        mixer_b.mix(stream_b1_handle, &stream_b1);
        mixer_c.mix(mixer_a_handle, &mixer_a);
        mixer_c.mix(mixer_b_handle, &mixer_b);

        Lowl::Audio::AudioBuffer buffer(frame_count, channel_count);
        Lowl::Audio::AudioBlockView block = buffer.view(frame_count);
        buffer.clear(frame_count);

        Lowl::Audio::AudioSource::RenderResult result = mixer_c.render(block);
        REQUIRE_EQ(result.frames_produced, frame_count);
        REQUIRE_EQ(result.state, Lowl::Audio::AudioSource::RenderState::Ok);
        for (uint32_t frame_index = 0; frame_index < frame_count; frame_index++) {
            for (uint8_t current_channel = 0; current_channel < channel_count; current_channel++) {
                const size_t sample_index = static_cast<size_t>(frame_index) * channel_count + current_channel;
                const Lowl::Sample expected = space_frames[sample_index] + stream_a_frames[sample_index] +
                                              stream_b0_frames[sample_index] + stream_b1_frames[sample_index];
                REQUIRE_EQ(block.channel(current_channel)[frame_index], doctest::Approx(expected));
            }
        }
    }
}
