#include <doctest/doctest.h>

#include "audio/lowl_audio_buffer.h"
#include "audio/backend/lowl_audio_device.h"

#include <array>
#include <memory>
#include <utility>
#include <vector>

namespace {
    struct StereoSample {
        Lowl::Sample left;
        Lowl::Sample right;
    };

    class ShortReadAudioSource final : public Lowl::Audio::AudioSource {
    public:
        explicit ShortReadAudioSource(std::vector<StereoSample> p_frames)
            : AudioSource(44100.0, Lowl::Audio::ChannelLayout::Stereo), frames(std::move(p_frames)) {
        }

        RenderResult render(Lowl::Audio::AudioBlockView p_block) override {
            if (next_frame >= frames.size() || p_block.frame_count == 0) {
                return {0, RenderState::Finished};
            }

            const uint32_t frames_to_copy = static_cast<uint32_t>(std::min<size_t>(frames.size() - next_frame, p_block.frame_count));
            for (uint32_t frame_index = 0; frame_index < frames_to_copy; frame_index++) {
                const StereoSample &frame = frames[next_frame + frame_index];
                p_block.channel(0)[frame_index] = frame.left;
                p_block.channel(1)[frame_index] = frame.right;
            }
            next_frame += frames_to_copy;
            if (next_frame >= frames.size()) {
                return {frames_to_copy, RenderState::Finished};
            }
            return {frames_to_copy, RenderState::Ok};
        }

        Lowl::size_l get_frames_remaining() const override {
            return frames.size() - next_frame;
        }

        Lowl::size_l get_frame_position() const override {
            return next_frame;
        }

        Lowl::size_l get_frame_count() const override {
            return frames.size();
        }

    private:
        std::vector<StereoSample> frames;
        Lowl::size_l next_frame = 0;
    };

    class TestAudioDevice final : public Lowl::Audio::AudioDevice {
    public:
        TestAudioDevice() : AudioDevice(_constructor_tag()) {
        }

        void configure(
            const Lowl::Audio::AudioDeviceProperties &p_properties,
            std::shared_ptr<Lowl::Audio::AudioSource> p_audio_source
        ) {
            audio_device_properties = p_properties;
            audio_source = std::move(p_audio_source);
        }

        void write(void *p_dst, unsigned long p_frames_per_buffer, unsigned long p_bytes_per_frame) {
            allocate_render_buffer(p_frames_per_buffer);
            render_to_device_buffer(p_dst, p_frames_per_buffer, p_bytes_per_frame);
        }

        void set_properties_list(std::vector<Lowl::Audio::AudioDeviceProperties> p_properties_list) {
            properties_list = std::move(p_properties_list);
        }

        void start(
            Lowl::Audio::AudioDeviceProperties,
            std::shared_ptr<Lowl::Audio::AudioSource>,
            Lowl::Error &
        ) override {
        }

        void stop(Lowl::Error &) override {
        }
    };
}

TEST_CASE("AudioDevice") {
    SUBCASE("AudioDeviceProperties - default initialization is safe") {
        Lowl::Audio::AudioDeviceProperties properties;
        REQUIRE_FALSE(properties.is_supported);
        REQUIRE_EQ(properties.sample_rate, Lowl::NO_SAMPLE_RATE);
        REQUIRE_FALSE(properties.channel_layout.is_valid());
        REQUIRE_EQ(properties.sample_format, Lowl::Audio::SampleFormat::Unknown);
        REQUIRE_FALSE(properties.exclusive_mode);
        REQUIRE_EQ(properties.wasapi.valid_bits_per_sample, 0);
    }

    SUBCASE("AudioDevice - get_closest_properties prefers exact structural matches") {
        TestAudioDevice device;

        Lowl::Audio::AudioDeviceProperties exact{};
        exact.is_supported = true;
        exact.sample_rate = 48000.0;
        exact.channel_layout = Lowl::Audio::ChannelLayout::Stereo;
        exact.sample_format = Lowl::Audio::SampleFormat::FLOAT_32;
        exact.exclusive_mode = false;

        Lowl::Audio::AudioDeviceProperties wrong_channel = exact;
        wrong_channel.channel_layout = Lowl::Audio::ChannelLayout::Mono;

        Lowl::Audio::AudioDeviceProperties wrong_format = exact;
        wrong_format.sample_format = Lowl::Audio::SampleFormat::INT_16;

        device.set_properties_list({wrong_channel, wrong_format, exact});

        Lowl::Error error;
        const Lowl::Audio::AudioDeviceProperties selected = device.get_closest_properties(exact, error);
        REQUIRE_FALSE(error.has_error());
        REQUIRE_EQ(selected, exact);
    }

    SUBCASE("AudioDevice - get_closest_properties falls back to nearest sample rate") {
        TestAudioDevice device;

        Lowl::Audio::AudioDeviceProperties requested{};
        requested.is_supported = true;
        requested.sample_rate = 50000.0;
        requested.channel_layout = Lowl::Audio::ChannelLayout::Stereo;
        requested.sample_format = Lowl::Audio::SampleFormat::FLOAT_32;

        Lowl::Audio::AudioDeviceProperties low = requested;
        low.sample_rate = 48000.0;

        Lowl::Audio::AudioDeviceProperties high = requested;
        high.sample_rate = 96000.0;

        device.set_properties_list({high, low});

        Lowl::Error error;
        const Lowl::Audio::AudioDeviceProperties selected = device.get_closest_properties(requested, error);
        REQUIRE_FALSE(error.has_error());
        REQUIRE_EQ(selected.sample_rate, doctest::Approx(48000.0));
    }

    SUBCASE("AudioDevice - write_frames only zero-fills missing frames") {
        constexpr unsigned long frames_per_buffer = 3;
        constexpr unsigned long channels = 2;
        constexpr unsigned long bytes_per_frame = sizeof(float) * channels;
        constexpr size_t audio_bytes = frames_per_buffer * bytes_per_frame;
        constexpr size_t guard_bytes = 16;

        alignas(float) std::array<uint8_t, audio_bytes + guard_bytes> buffer{};
        buffer.fill(0x7F);

        auto source = std::make_shared<ShortReadAudioSource>(
            std::vector<StereoSample>{StereoSample{0.25f, -0.25f}}
        );

        Lowl::Audio::AudioDeviceProperties properties{};
        properties.sample_format = Lowl::Audio::SampleFormat::FLOAT_32;
        properties.channel_layout = Lowl::Audio::ChannelLayout::Stereo;

        TestAudioDevice device;
        device.configure(properties, source);
        device.write(buffer.data(), frames_per_buffer, bytes_per_frame);

        auto *samples = reinterpret_cast<const float *>(buffer.data());
        REQUIRE_EQ(samples[0], doctest::Approx(0.25f));
        REQUIRE_EQ(samples[1], doctest::Approx(-0.25f));

        for (size_t current_sample = channels; current_sample < frames_per_buffer * channels; current_sample++) {
            REQUIRE_EQ(samples[current_sample], doctest::Approx(0.0f));
        }

        for (size_t current_byte = audio_bytes; current_byte < buffer.size(); current_byte++) {
            REQUIRE_EQ(buffer[current_byte], static_cast<uint8_t>(0x7F));
        }
    }
}
