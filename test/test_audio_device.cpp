#include <doctest/doctest.h>

#include "audio/backend/lowl_audio_device.h"

#include <array>
#include <memory>
#include <utility>
#include <vector>

namespace {
    class ShortReadAudioSource final : public Lowl::Audio::AudioSource {
    public:
        explicit ShortReadAudioSource(std::vector<Lowl::Audio::AudioFrame> p_frames)
            : AudioSource(44100.0, Lowl::Audio::AudioChannel::Stereo), frames(std::move(p_frames)) {
        }

        ReadResult read(Lowl::Audio::AudioFrame &p_audio_frame) override {
            if (next_frame >= frames.size()) {
                return ReadResult::End;
            }

            p_audio_frame = frames[next_frame++];
            return ReadResult::Read;
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
        std::vector<Lowl::Audio::AudioFrame> frames;
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

        void write(void *p_dst, unsigned long p_frames_per_buffer, unsigned long p_bytes_per_frame) const {
            write_frames(p_dst, p_frames_per_buffer, p_bytes_per_frame);
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
    SUBCASE("AudioDevice - write_frames only zero-fills missing frames") {
        constexpr unsigned long frames_per_buffer = 3;
        constexpr unsigned long channels = 2;
        constexpr unsigned long bytes_per_frame = sizeof(float) * channels;
        constexpr size_t audio_bytes = frames_per_buffer * bytes_per_frame;
        constexpr size_t guard_bytes = 16;

        alignas(float) std::array<uint8_t, audio_bytes + guard_bytes> buffer{};
        buffer.fill(0x7F);

        auto source = std::make_shared<ShortReadAudioSource>(
            std::vector<Lowl::Audio::AudioFrame>{Lowl::Audio::AudioFrame(0.25f, -0.25f)}
        );

        Lowl::Audio::AudioDeviceProperties properties{};
        properties.sample_format = Lowl::Audio::SampleFormat::FLOAT_32;
        properties.channel = Lowl::Audio::AudioChannel::Stereo;

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
