#include <doctest/doctest.h>

#include <lowl.h>
#include "audio/lowl_audio_buffer.h"

#include <iostream>
#include <memory>

namespace {
    struct StereoSample {
        Lowl::Sample left;
        Lowl::Sample right;
    };

    class MixerDetachProbe final : public Lowl::Audio::AudioSource {
    public:
        explicit MixerDetachProbe(Lowl::Audio::AudioChannel p_channel)
            : AudioSource(44100.0, p_channel) {
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
            : AudioSource(44100.0, Lowl::Audio::AudioChannel::Stereo),
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
        Lowl::Audio::AudioBuffer buffer(1, static_cast<uint8_t>(p_audio_stream.get_channel_num()));
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
            = std::make_unique<Lowl::Audio::AudioStream>(44100.0, Lowl::Audio::AudioChannel::Stereo);

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
        Lowl::Audio::AudioStream small_stream(44100.0, Lowl::Audio::AudioChannel::Stereo, 3);
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
        Lowl::Audio::AudioStream small_stream(44100.0, Lowl::Audio::AudioChannel::Stereo, 3);
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

    SUBCASE("AudioStream - render rejects mismatched channel block") {
        Lowl::Audio::AudioStream mono_stream(44100.0, Lowl::Audio::AudioChannel::Mono, 3);
        const Lowl::Sample samples[] = {0.5f};
        REQUIRE_EQ(mono_stream.write_interleaved(samples, 1), 1U);

        Lowl::Audio::AudioBuffer stereo_buffer(1, 2);
        Lowl::Audio::AudioBlockView stereo_block = stereo_buffer.view(1);
        stereo_buffer.clear(1);

        Lowl::Audio::AudioSource::RenderResult result = mono_stream.render(stereo_block);
        REQUIRE_EQ(result.frames_produced, 0U);
        REQUIRE_EQ(result.state, Lowl::Audio::AudioSource::RenderState::Starved);
        REQUIRE_EQ(stereo_block.channel(0)[0], doctest::Approx(0.0f));
        REQUIRE_EQ(stereo_block.channel(1)[0], doctest::Approx(0.0f));
    }

    SUBCASE("AudioMixer - mix rejects mismatched channel source") {
        Lowl::Audio::AudioMixer mixer(44100.0, Lowl::Audio::AudioChannel::Stereo);
        MixerDetachProbe mono_probe(Lowl::Audio::AudioChannel::Mono);

        mixer.mix(&mono_probe);

        REQUIRE(mono_probe.detached);
    }

    SUBCASE("AudioMixer - remove reuses a freed slot") {
        Lowl::Audio::AudioMixer mixer(44100.0, Lowl::Audio::AudioChannel::Stereo);
        ConstantMixerSource source_a({0.25f, 0.25f});
        ConstantMixerSource source_b({0.50f, 0.50f});
        ConstantMixerSource source_c({0.75f, 0.75f});

        Lowl::Audio::AudioBuffer buffer(1, 2);
        Lowl::Audio::AudioBlockView block = buffer.view(1);

        mixer.mix(&source_a);
        mixer.mix(&source_b);

        buffer.clear(1);
        Lowl::Audio::AudioSource::RenderResult first_result = mixer.render(block);
        REQUIRE_EQ(first_result.frames_produced, 1U);
        REQUIRE_EQ(first_result.state, Lowl::Audio::AudioSource::RenderState::Ok);
        REQUIRE_EQ(block.channel(0)[0], doctest::Approx(0.75f));
        REQUIRE_EQ(block.channel(1)[0], doctest::Approx(0.75f));

        mixer.remove(&source_b);
        mixer.mix(&source_c);

        buffer.clear(1);
        Lowl::Audio::AudioSource::RenderResult second_result = mixer.render(block);
        REQUIRE_EQ(second_result.frames_produced, 1U);
        REQUIRE_EQ(second_result.state, Lowl::Audio::AudioSource::RenderState::Ok);
        REQUIRE_EQ(block.channel(0)[0], doctest::Approx(1.0f));
        REQUIRE_EQ(block.channel(1)[0], doctest::Approx(1.0f));
    }
}
