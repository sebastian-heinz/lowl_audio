#include <doctest/doctest.h>
#include <lowl.h>

#include <chrono>
#include <iostream>
#include <memory>
#include <thread>
#include <vector>

#include "audio/lowl_audio_buffer.h"

namespace {
    struct StereoSample {
        Lowl::Sample left;
        Lowl::Sample right;
    };

    Lowl::Audio::AudioFormat make_audio_format(const Lowl::Audio::ChannelLayout p_channel_layout,
                                               const Lowl::SampleRate p_sample_rate = 44100.0) {
        return {p_sample_rate, p_channel_layout};
    }

    std::unique_ptr<Lowl::Audio::AudioData> make_audio_data(Lowl::Audio::ChannelLayout p_channel_layout,
                                                            const std::vector<Lowl::Sample> &p_interleaved_frames) {
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
        return std::make_unique<Lowl::Audio::AudioData>(
            std::move(storage), frame_count, make_audio_format(p_channel_layout));
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
            std::move(storage), frame_count, make_audio_format(Lowl::Audio::ChannelLayout::Stereo));
    }

    Lowl::AudioPlaybackHandle add_asset_and_create_playback(Lowl::Audio::AudioSpace &p_audio_space,
                                                            std::unique_ptr<Lowl::Audio::AudioData> p_audio_data,
                                                            Lowl::Error &p_error) {
        const Lowl::AudioAssetHandle asset_handle = p_audio_space.add_audio(std::move(p_audio_data), p_error);
        if (p_error.has_error() || !asset_handle.is_valid()) {
            return Lowl::Audio::AudioSpace::InvalidAudioPlaybackHandle;
        }
        return p_audio_space.create_playback(asset_handle, p_error);
    }

    Lowl::AudioMixerHandle
    connect_mixer_source(Lowl::Audio::AudioMixer &p_mixer, Lowl::Audio::AudioSource &p_source, Lowl::Error &p_error) {
        const Lowl::AudioMixerHandle handle = p_mixer.connect(p_source, p_error);
        REQUIRE_FALSE(p_error.has_error());
        REQUIRE(handle.is_valid());
        return handle;
    }

    void expect_mixer_completion(Lowl::Audio::AudioMixer &p_mixer,
                                 const Lowl::AudioMixerHandle p_handle,
                                 const Lowl::Audio::AudioMixerCompletion::Type p_type) {
        Lowl::Audio::AudioMixerCompletion completion{};
        REQUIRE(p_mixer.try_collect_completion(completion));
        REQUIRE(completion.handle == p_handle);
        REQUIRE_EQ(completion.type, p_type);
    }

    class MixerDetachProbe final : public Lowl::Audio::AudioSource {
    public:
        explicit MixerDetachProbe(Lowl::Audio::ChannelLayout p_channel_layout,
                                  const Lowl::SampleRate p_sample_rate = 44100.0)
            : AudioSource(make_audio_format(p_channel_layout, p_sample_rate)) {
        }

        RenderResult mix_into(Lowl::Audio::AudioBlockView, const MixGainVector &) override {
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
    };

    class ConstantMixerSource final : public Lowl::Audio::AudioSource {
    public:
        explicit ConstantMixerSource(const StereoSample p_sample)
            : AudioSource(make_audio_format(Lowl::Audio::ChannelLayout::Stereo)), sample(p_sample) {
        }

        RenderResult mix_into(Lowl::Audio::AudioBlockView p_block, const MixGainVector &) override {
            if (p_block.channel_count != 2 || p_block.frame_count == 0) {
                return {0, RenderState::Starved};
            }
            p_block.channel(0)[0] += sample.left;
            p_block.channel(1)[0] += sample.right;
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
} // namespace

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

    std::shared_ptr<Lowl::Audio::AudioStream> audio_stream =
        std::make_unique<Lowl::Audio::AudioStream>(make_audio_format(Lowl::Audio::ChannelLayout::Stereo));

    SUBCASE("AudioMixer - aggregate frame queries use a consistent live sentinel") {
        Lowl::Audio::AudioMixer mixer(make_audio_format(Lowl::Audio::ChannelLayout::Stereo));

        REQUIRE_EQ(mixer.get_frames_remaining(), 1U);
        REQUIRE_EQ(mixer.get_frame_count(), 1U);
        REQUIRE_EQ(mixer.get_frame_position(), 0U);
    }

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
        Lowl::Audio::AudioStream small_stream(make_audio_format(Lowl::Audio::ChannelLayout::Stereo), 3);
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
        Lowl::Audio::AudioStream small_stream(make_audio_format(Lowl::Audio::ChannelLayout::Stereo), 3);
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
        Lowl::Audio::AudioStream small_stream(make_audio_format(Lowl::Audio::ChannelLayout::Stereo), 2);
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

    SUBCASE("AudioStream - requested capacity remains the writable limit") {
        Lowl::Audio::AudioStream small_stream(make_audio_format(Lowl::Audio::ChannelLayout::Stereo), 3);
        const Lowl::Sample frames[] = {0.1f, -0.1f, 0.2f, -0.2f, 0.3f, -0.3f, 0.4f, -0.4f};

        REQUIRE_EQ(small_stream.write_interleaved(frames, 4), 3U);

        auto [result0, read0] = render_one_frame(small_stream);
        REQUIRE_EQ(result0.frames_produced, 1U);
        REQUIRE_EQ(read0.left, doctest::Approx(0.1f));
        REQUIRE_EQ(read0.right, doctest::Approx(-0.1f));

        auto [result1, read1] = render_one_frame(small_stream);
        REQUIRE_EQ(result1.frames_produced, 1U);
        REQUIRE_EQ(read1.left, doctest::Approx(0.2f));
        REQUIRE_EQ(read1.right, doctest::Approx(-0.2f));

        auto [result2, read2] = render_one_frame(small_stream);
        REQUIRE_EQ(result2.frames_produced, 1U);
        REQUIRE_EQ(read2.left, doctest::Approx(0.3f));
        REQUIRE_EQ(read2.right, doctest::Approx(-0.3f));

        auto [result3, read3] = render_one_frame(small_stream);
        REQUIRE_EQ(result3.frames_produced, 0U);
        REQUIRE_EQ(result3.state, Lowl::Audio::AudioSource::RenderState::Starved);
        REQUIRE_EQ(read3.left, doctest::Approx(0.0f));
        REQUIRE_EQ(read3.right, doctest::Approx(0.0f));
    }

    SUBCASE("AudioStream - concurrent single producer and single consumer preserve frame order") {
        constexpr size_t total_frames = 64;
        Lowl::Audio::AudioStream stream(make_audio_format(Lowl::Audio::ChannelLayout::Stereo), 4);
        std::vector<StereoSample> expected_frames(total_frames);
        for (size_t frame_index = 0; frame_index < total_frames; frame_index++) {
            expected_frames[frame_index] = {static_cast<Lowl::Sample>(frame_index) / 100.0f,
                                            static_cast<Lowl::Sample>(-static_cast<int>(frame_index)) / 100.0f};
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
        for (size_t frame_index = 0; frame_index < total_frames;) {
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
        Lowl::Audio::AudioStream mono_stream(make_audio_format(Lowl::Audio::ChannelLayout::Mono), 3);
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
        Lowl::Audio::AudioMixer mixer(make_audio_format(Lowl::Audio::ChannelLayout::Stereo));

        Lowl::Audio::AudioBuffer mono_buffer(1, 1);
        Lowl::Audio::AudioBlockView mono_block = mono_buffer.view(1);
        mono_buffer.clear(1);

        Lowl::Audio::AudioSource::RenderResult result = mixer.render(mono_block);
        REQUIRE_EQ(result.frames_produced, 0U);
        REQUIRE_EQ(result.state, Lowl::Audio::AudioSource::RenderState::Error);
        REQUIRE_EQ(mono_block.channel(0)[0], doctest::Approx(0.0f));
    }

    SUBCASE("AudioMixer - connect rejects mismatched channel source synchronously") {
        Lowl::Audio::AudioMixer mixer(make_audio_format(Lowl::Audio::ChannelLayout::Stereo));
        MixerDetachProbe mono_probe(Lowl::Audio::ChannelLayout::Mono);
        Lowl::Error error;

        const Lowl::AudioMixerHandle mono_handle = mixer.connect(mono_probe, error);

        REQUIRE_FALSE(mono_handle.is_valid());
        REQUIRE_EQ(error.get_error(), Lowl::ErrorCode::UnsupportedAudioFormat);
        Lowl::Audio::AudioMixerCompletion completion{};
        REQUIRE_FALSE(mixer.try_collect_completion(completion));
    }

    SUBCASE("AudioMixer - connect rejects mismatched sample-rate source synchronously") {
        Lowl::Audio::AudioMixer mixer(make_audio_format(Lowl::Audio::ChannelLayout::Stereo));
        MixerDetachProbe probe(Lowl::Audio::ChannelLayout::Stereo, 48000.0);
        Lowl::Error error;

        const Lowl::AudioMixerHandle handle = mixer.connect(probe, error);

        REQUIRE_FALSE(handle.is_valid());
        REQUIRE_EQ(error.get_error(), Lowl::ErrorCode::UnsupportedAudioFormat);
        Lowl::Audio::AudioMixerCompletion completion{};
        REQUIRE_FALSE(mixer.try_collect_completion(completion));
    }

    SUBCASE("AudioMixer - collected removal reuses a freed slot") {
        Lowl::Audio::AudioMixer mixer(make_audio_format(Lowl::Audio::ChannelLayout::Stereo));
        ConstantMixerSource source_a({0.25f, 0.25f});
        ConstantMixerSource source_b({0.50f, 0.50f});
        ConstantMixerSource source_c({0.75f, 0.75f});
        Lowl::Error error;
        const Lowl::AudioMixerHandle handle_a = connect_mixer_source(mixer, source_a, error);
        const Lowl::AudioMixerHandle handle_b = connect_mixer_source(mixer, source_b, error);

        Lowl::Audio::AudioBuffer buffer(1, 2);
        Lowl::Audio::AudioBlockView block = buffer.view(1);

        buffer.clear(1);
        Lowl::Audio::AudioSource::RenderResult first_result = mixer.render(block);
        REQUIRE_EQ(first_result.frames_produced, 1U);
        REQUIRE_EQ(first_result.state, Lowl::Audio::AudioSource::RenderState::Ok);
        REQUIRE_EQ(block.channel(0)[0], doctest::Approx(0.75f));
        REQUIRE_EQ(block.channel(1)[0], doctest::Approx(0.75f));

        mixer.disconnect(handle_b, error);
        REQUIRE_FALSE(error.has_error());

        buffer.clear(1);
        const Lowl::Audio::AudioSource::RenderResult removal_result = mixer.render(block);
        REQUIRE_EQ(removal_result.frames_produced, 1U);
        REQUIRE_EQ(removal_result.state, Lowl::Audio::AudioSource::RenderState::Ok);
        REQUIRE_EQ(block.channel(0)[0], doctest::Approx(0.25f));
        REQUIRE_EQ(block.channel(1)[0], doctest::Approx(0.25f));
        expect_mixer_completion(mixer, handle_b, Lowl::Audio::AudioMixerCompletion::Type::Removed);

        const Lowl::AudioMixerHandle handle_c = connect_mixer_source(mixer, source_c, error);
        REQUIRE_EQ(handle_c.connection_id, handle_b.connection_id);
        REQUIRE_NE(handle_c.generation, handle_b.generation);

        buffer.clear(1);
        Lowl::Audio::AudioSource::RenderResult second_result = mixer.render(block);
        REQUIRE_EQ(second_result.frames_produced, 1U);
        REQUIRE_EQ(second_result.state, Lowl::Audio::AudioSource::RenderState::Ok);
        REQUIRE_EQ(block.channel(0)[0], doctest::Approx(1.0f));
        REQUIRE_EQ(block.channel(1)[0], doctest::Approx(1.0f));
    }

    SUBCASE("AudioMixer - stale completed handles are rejected") {
        Lowl::Audio::AudioMixer mixer(make_audio_format(Lowl::Audio::ChannelLayout::Stereo));
        ConstantMixerSource source_a({0.25f, 0.25f});
        ConstantMixerSource source_b({0.50f, 0.50f});
        Lowl::Error error;
        const Lowl::AudioMixerHandle first_handle = connect_mixer_source(mixer, source_a, error);

        Lowl::Audio::AudioBuffer buffer(1, 2);
        Lowl::Audio::AudioBlockView block = buffer.view(1);
        buffer.clear(1);
        REQUIRE_EQ(mixer.render(block).frames_produced, 1U);

        mixer.disconnect(first_handle, error);
        REQUIRE_FALSE(error.has_error());
        buffer.clear(1);
        mixer.render(block);
        expect_mixer_completion(mixer, first_handle, Lowl::Audio::AudioMixerCompletion::Type::Removed);

        mixer.disconnect(first_handle, error);
        REQUIRE_EQ(error.get_error(), Lowl::ErrorCode::MixerConnectionInvalid);

        const Lowl::AudioMixerHandle second_handle = connect_mixer_source(mixer, source_b, error);
        REQUIRE_EQ(second_handle.connection_id, first_handle.connection_id);
        REQUIRE_NE(second_handle.generation, first_handle.generation);
    }

    SUBCASE("AudioMixer - repeated disconnect requests fail without stopping the mixer") {
        Lowl::Audio::AudioMixer mixer(make_audio_format(Lowl::Audio::ChannelLayout::Stereo));
        ConstantMixerSource source({0.25f, 0.25f});
        Lowl::Error error;
        const Lowl::AudioMixerHandle handle = connect_mixer_source(mixer, source, error);

        Lowl::Audio::AudioBuffer buffer(1, 2);
        Lowl::Audio::AudioBlockView block = buffer.view(1);
        buffer.clear(1);
        REQUIRE_EQ(mixer.render(block).frames_produced, 1U);

        mixer.disconnect(handle, error);
        REQUIRE_FALSE(error.has_error());
        mixer.disconnect(handle, error);
        REQUIRE_EQ(error.get_error(), Lowl::ErrorCode::InvalidOperationWhileActive);

        buffer.clear(1);
        mixer.render(block);
        expect_mixer_completion(mixer, handle, Lowl::Audio::AudioMixerCompletion::Type::Removed);
    }

    SUBCASE("AudioMixer - handles are scoped to one mixer") {
        Lowl::Audio::AudioMixer first_mixer(make_audio_format(Lowl::Audio::ChannelLayout::Stereo));
        Lowl::Audio::AudioMixer second_mixer(make_audio_format(Lowl::Audio::ChannelLayout::Stereo));
        ConstantMixerSource source({0.25f, 0.25f});
        Lowl::Error error;
        const Lowl::AudioMixerHandle first_handle = connect_mixer_source(first_mixer, source, error);

        second_mixer.disconnect(first_handle, error);
        REQUIRE_EQ(error.get_error(), Lowl::ErrorCode::MixerConnectionInvalid);

        const Lowl::AudioMixerHandle second_handle = connect_mixer_source(second_mixer, source, error);
        REQUIRE_EQ(first_handle.connection_id, second_handle.connection_id);
        REQUIRE_EQ(first_handle.generation, second_handle.generation);
        REQUIRE_NE(first_handle.mixer_id, second_handle.mixer_id);

        Lowl::Audio::AudioBuffer buffer(1, 2);
        Lowl::Audio::AudioBlockView block = buffer.view(1);
        buffer.clear(1);
        const Lowl::Audio::AudioSource::RenderResult result = second_mixer.render(block);
        REQUIRE_EQ(result.frames_produced, 1U);
        REQUIRE_EQ(block.channel(0)[0], doctest::Approx(0.25f));
        REQUIRE_EQ(block.channel(1)[0], doctest::Approx(0.25f));
    }

    SUBCASE("AudioMixer - collected handle slots are reused with a new generation") {
        Lowl::Audio::AudioMixer mixer(make_audio_format(Lowl::Audio::ChannelLayout::Stereo));
        ConstantMixerSource first_source({0.25f, 0.25f});
        ConstantMixerSource second_source({0.50f, 0.50f});
        Lowl::Error error;
        const Lowl::AudioMixerHandle first_handle = connect_mixer_source(mixer, first_source, error);

        Lowl::Audio::AudioBuffer buffer(1, 2);
        Lowl::Audio::AudioBlockView block = buffer.view(1);
        buffer.clear(1);
        mixer.render(block);
        mixer.disconnect(first_handle, error);
        REQUIRE_FALSE(error.has_error());
        buffer.clear(1);
        mixer.render(block);
        expect_mixer_completion(mixer, first_handle, Lowl::Audio::AudioMixerCompletion::Type::Removed);

        const Lowl::AudioMixerHandle second_handle = connect_mixer_source(mixer, second_source, error);
        REQUIRE_EQ(second_handle.connection_id, first_handle.connection_id);
        REQUIRE_NE(second_handle.generation, first_handle.generation);
    }

    SUBCASE("AudioMixer - rejects sources beyond fixed connection capacity") {
        Lowl::Audio::AudioMixer mixer(make_audio_format(Lowl::Audio::ChannelLayout::Stereo));
        Lowl::Error error;

        std::vector<Lowl::AudioMixerHandle> handles;
        std::vector<std::unique_ptr<ConstantMixerSource>> sources;
        handles.reserve(Lowl::Audio::AudioMixer::MaxConnections);
        sources.reserve(Lowl::Audio::AudioMixer::MaxConnections + 1);

        for (size_t index = 0; index < Lowl::Audio::AudioMixer::MaxConnections; index++) {
            sources.push_back(std::make_unique<ConstantMixerSource>(StereoSample{1.0f, 1.0f}));
            handles.push_back(connect_mixer_source(mixer, *sources.back(), error));
        }
        sources.push_back(std::make_unique<ConstantMixerSource>(StereoSample{1.0f, 1.0f}));
        const Lowl::AudioMixerHandle rejected_handle = mixer.connect(*sources.back(), error);
        REQUIRE_FALSE(rejected_handle.is_valid());
        REQUIRE_EQ(error.get_error(), Lowl::ErrorCode::MixerCapacityExhausted);

        Lowl::Audio::AudioBuffer buffer(1, 2);
        Lowl::Audio::AudioBlockView block = buffer.view(1);
        Lowl::Audio::AudioSource::RenderResult result{};
        const size_t render_pass_count =
            (Lowl::Audio::AudioMixer::MaxConnections + Lowl::Audio::AudioMixer::MaxEventsPerRender - 1) /
            Lowl::Audio::AudioMixer::MaxEventsPerRender;
        for (size_t pass = 0; pass < render_pass_count; pass++) {
            buffer.clear(1);
            result = mixer.render(block);
        }
        REQUIRE_EQ(result.frames_produced, 1U);
        REQUIRE_EQ(result.state, Lowl::Audio::AudioSource::RenderState::Ok);
        REQUIRE_EQ(block.channel(0)[0], doctest::Approx(1024.0f));
        REQUIRE_EQ(block.channel(1)[0], doctest::Approx(1024.0f));
    }

    SUBCASE("AudioMixer - renders the requested block directly") {
        Lowl::Audio::AudioMixer mixer(make_audio_format(Lowl::Audio::ChannelLayout::Stereo));
        Lowl::Audio::AudioStream stereo_stream(make_audio_format(Lowl::Audio::ChannelLayout::Stereo), 8);
        Lowl::Error error;
        const Lowl::AudioMixerHandle stream_handle = connect_mixer_source(mixer, stereo_stream, error);
        const Lowl::Sample samples[] = {
            0.1f,
            -0.1f,
            0.2f,
            -0.2f,
            0.3f,
            -0.3f,
            0.4f,
            -0.4f,
            0.5f,
            -0.5f,
        };

        REQUIRE_EQ(stereo_stream.write_interleaved(samples, 5), 5U);
        REQUIRE(stream_handle.is_valid());

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
        Lowl::Audio::AudioSpace audio_space(make_audio_format(channel));
        Lowl::Audio::AudioMixer mixer_a(make_audio_format(channel));
        Lowl::Audio::AudioMixer mixer_b(make_audio_format(channel));
        Lowl::Audio::AudioMixer mixer_c(make_audio_format(channel));
        Lowl::Audio::AudioStream stream_a(make_audio_format(channel), 16);
        Lowl::Audio::AudioStream stream_b0(make_audio_format(channel), 16);
        Lowl::Audio::AudioStream stream_b1(make_audio_format(channel), 16);
        const Lowl::AudioMixerHandle audio_space_handle = connect_mixer_source(mixer_a, audio_space, error);
        const Lowl::AudioMixerHandle stream_a_handle = connect_mixer_source(mixer_a, stream_a, error);
        const Lowl::AudioMixerHandle stream_b0_handle = connect_mixer_source(mixer_b, stream_b0, error);
        const Lowl::AudioMixerHandle stream_b1_handle = connect_mixer_source(mixer_b, stream_b1, error);
        const Lowl::AudioMixerHandle mixer_a_handle = connect_mixer_source(mixer_c, mixer_a, error);
        const Lowl::AudioMixerHandle mixer_b_handle = connect_mixer_source(mixer_c, mixer_b, error);

        const std::vector<Lowl::Sample> space_frames = make_interleaved_frames(100, 10, 1);
        const std::vector<Lowl::Sample> stream_a_frames = make_interleaved_frames(20, 4, 2);
        const std::vector<Lowl::Sample> stream_b0_frames = make_interleaved_frames(-30, 2, 1);
        const std::vector<Lowl::Sample> stream_b1_frames = make_interleaved_frames(5, -1, 3);

        const Lowl::AudioPlaybackHandle playback_handle =
            add_asset_and_create_playback(audio_space, make_audio_data(channel, space_frames), error);

        REQUIRE_FALSE(error.has_error());
        REQUIRE(playback_handle.is_valid());

        REQUIRE_EQ(stream_a.write_interleaved(stream_a_frames.data(), frame_count), frame_count);
        REQUIRE_EQ(stream_b0.write_interleaved(stream_b0_frames.data(), frame_count), frame_count);
        REQUIRE_EQ(stream_b1.write_interleaved(stream_b1_frames.data(), frame_count), frame_count);

        audio_space.play(playback_handle, error);
        REQUIRE_FALSE(error.has_error());
        REQUIRE(audio_space_handle.is_valid());
        REQUIRE(stream_a_handle.is_valid());
        REQUIRE(stream_b0_handle.is_valid());
        REQUIRE(stream_b1_handle.is_valid());
        REQUIRE(mixer_a_handle.is_valid());
        REQUIRE(mixer_b_handle.is_valid());

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
