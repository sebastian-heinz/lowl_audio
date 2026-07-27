#include <doctest/doctest.h>

#ifdef LOWL_DRIVER_CORE_AUDIO

#include <array>
#include <cstddef>
#include <cstring>
#include <memory>
#include <utility>
#include <vector>

#include "audio/backend/coreaudio/lowl_audio_core_audio_device.h"
#include "audio/backend/coreaudio/lowl_audio_core_audio_layout.h"
#include "audio/lowl_audio_setting.h"
#include "audio/source/lowl_audio_mixer.h"

namespace {
    struct StereoSample {
        Lowl::Sample left;
        Lowl::Sample right;
    };

    class OneFrameStereoSource final : public Lowl::Audio::AudioSource {
    public:
        explicit OneFrameStereoSource(const StereoSample p_sample)
            : AudioSource(Lowl::Audio::AudioFormat{44100.0, Lowl::Audio::ChannelLayout::Stereo}), sample(p_sample) {
        }

        RenderResult mix_into(Lowl::Audio::AudioBlockView p_block, const MixGainVector &) override {
            if (consumed || p_block.frame_count == 0) {
                return {0, RenderState::Finished};
            }
            p_block.channel(0)[0] += sample.left;
            p_block.channel(1)[0] += sample.right;
            consumed = true;
            return {1, RenderState::Finished};
        }

        Lowl::size_l get_frames_remaining() const override {
            return consumed ? 0 : 1;
        }

        Lowl::size_l get_frame_position() const override {
            return consumed ? 1 : 0;
        }

        Lowl::size_l get_frame_count() const override {
            return 1;
        }

    private:
        StereoSample sample;
        bool consumed = false;
    };

    template <typename Value> Value read_unaligned(const uint8_t *p_src) {
        Value value{};
        std::memcpy(&value, p_src, sizeof(value));
        return value;
    }

    class RecordingCoreAudioDevice final : public Lowl::Audio::CoreAudioDevice {
    public:
        RecordingCoreAudioDevice() : CoreAudioDevice(_constructor_tag{}) {
        }

        void configure(const Lowl::Audio::AudioDeviceProperties &p_properties,
                       std::shared_ptr<Lowl::Audio::AudioSource> p_audio_source) {
            audio_device_properties = p_properties;
            audio_source = std::move(p_audio_source);
        }

        bool prepare(const unsigned long p_frames_per_buffer) {
            return allocate_render_buffer(p_frames_per_buffer);
        }

        const std::vector<AudioObjectPropertySelector> &get_seen_selectors() const {
            return seen_selectors;
        }

    protected:
        void handle_property_address(const AudioObjectPropertyAddress &p_address) override {
            seen_selectors.push_back(p_address.mSelector);
            CoreAudioDevice::handle_property_address(p_address);
        }

    private:
        std::vector<AudioObjectPropertySelector> seen_selectors;
    };
} // namespace

TEST_CASE("CoreAudioDevice") {
    SUBCASE("CoreAudioDevice - every advertised channel layout round-trips through CoreAudio descriptions") {
        const std::vector<Lowl::Audio::ChannelLayout> advertised_layouts =
            Lowl::Audio::AudioSetting::get_test_channel_layouts();

        for (const Lowl::Audio::ChannelLayout &layout : advertised_layouts) {
            CAPTURE(layout.to_string());
            const std::vector<uint8_t> layout_data = Lowl::Audio::CoreAudioLayout::create_channel_layout_data(layout);
            REQUIRE_FALSE(layout_data.empty());

            const auto *core_audio_layout = reinterpret_cast<const AudioChannelLayout *>(layout_data.data());
            REQUIRE_EQ(core_audio_layout->mChannelLayoutTag, kAudioChannelLayoutTag_UseChannelDescriptions);
            REQUIRE_EQ(core_audio_layout->mNumberChannelDescriptions, layout.channel_count);
            REQUIRE_EQ(Lowl::Audio::CoreAudioLayout::to_channel_layout(*core_audio_layout), layout);
        }
    }

    SUBCASE("CoreAudioDevice - property_callback processes every address in the batch") {
        RecordingCoreAudioDevice device;
        const AudioObjectPropertyAddress addresses[] = {
            {kAudioObjectPropertyName, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMain},
            {kAudioDeviceProcessorOverload, kAudioDevicePropertyScopeOutput, kAudioObjectPropertyElementMain},
            {kAudioDevicePropertyNominalSampleRate, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMain},
        };

        REQUIRE_EQ(device.property_callback(kAudioObjectSystemObject, 3, addresses), noErr);
        REQUIRE_EQ(device.get_seen_selectors().size(), 3);
        REQUIRE_EQ(device.get_seen_selectors()[0], kAudioObjectPropertyName);
        REQUIRE_EQ(device.get_seen_selectors()[1], kAudioDeviceProcessorOverload);
        REQUIRE_EQ(device.get_seen_selectors()[2], kAudioDevicePropertyNominalSampleRate);
    }

    SUBCASE("CoreAudioDevice - property_callback ignores empty batches safely") {
        RecordingCoreAudioDevice device;
        const AudioObjectPropertyAddress dummy_address = {
            kAudioObjectPropertyName,
            kAudioObjectPropertyScopeGlobal,
            kAudioObjectPropertyElementMain,
        };

        REQUIRE_EQ(device.property_callback(kAudioObjectSystemObject, 0, &dummy_address), noErr);
        REQUIRE(device.get_seen_selectors().empty());
    }

    SUBCASE("CoreAudioDevice - start rejects unsupported sample formats before CoreAudio setup") {
        RecordingCoreAudioDevice device;
        const Lowl::Audio::SampleFormat unsupported_formats[] = {
            Lowl::Audio::SampleFormat::U_INT_8,
            Lowl::Audio::SampleFormat::Unknown,
        };

        for (const Lowl::Audio::SampleFormat sample_format : unsupported_formats) {
            Lowl::Audio::AudioDeviceProperties properties{};
            properties.is_supported = true;
            properties.audio_format = Lowl::Audio::AudioFormat{44100.0, Lowl::Audio::ChannelLayout::Stereo};
            properties.sample_format = sample_format;

            Lowl::Error error;
            device.start(properties, nullptr, error);

            REQUIRE(error.has_error());
            REQUIRE_EQ(error.get_error(), Lowl::ErrorCode::UnsupportedAudioFormat);

            Lowl::Error stop_error;
            device.stop(stop_error);
            REQUIRE_FALSE(stop_error.has_error());
        }
    }

    SUBCASE("CoreAudioDevice - start rejects a source with a different graph AudioFormat") {
        RecordingCoreAudioDevice device;
        Lowl::Audio::AudioDeviceProperties properties{};
        properties.is_supported = true;
        properties.audio_format = Lowl::Audio::AudioFormat{44100.0, Lowl::Audio::ChannelLayout::Stereo};
        properties.sample_format = Lowl::Audio::SampleFormat::FLOAT_32;

        auto source = std::make_shared<Lowl::Audio::AudioMixer>(
            Lowl::Audio::AudioFormat{48000.0, Lowl::Audio::ChannelLayout::Stereo});
        Lowl::Error error;
        device.start(properties, source, error);

        REQUIRE(error.has_error());
        REQUIRE_EQ(error.get_error(), Lowl::ErrorCode::InvalidParameter);
    }

    SUBCASE("CoreAudioDevice - callback converts planar floating buffers from Sample storage") {
        constexpr UInt32 frames_per_buffer = 2;
        constexpr UInt32 channels = 2;
        constexpr size_t max_channel_bytes = frames_per_buffer * sizeof(double);
        constexpr size_t guard_bytes = 16;
        constexpr size_t audio_buffer_list_bytes =
            offsetof(AudioBufferList, mBuffers) + channels * sizeof(::AudioBuffer);
        const std::array<Lowl::Audio::SampleFormat, 2> sample_formats = {
            Lowl::Audio::SampleFormat::FLOAT_32,
            Lowl::Audio::SampleFormat::FLOAT_64,
        };

        for (const Lowl::Audio::SampleFormat sample_format : sample_formats) {
            alignas(double) std::array<uint8_t, max_channel_bytes + guard_bytes> left_buffer{};
            alignas(double) std::array<uint8_t, max_channel_bytes + guard_bytes> right_buffer{};
            alignas(AudioBufferList) std::array<std::byte, audio_buffer_list_bytes> buffer_list_storage{};
            left_buffer.fill(0x7F);
            right_buffer.fill(0x7F);

            const size_t sample_size = Lowl::Audio::get_sample_size_bytes(sample_format);
            const size_t channel_bytes = frames_per_buffer * sample_size;
            auto *buffer_list = reinterpret_cast<AudioBufferList *>(buffer_list_storage.data());
            buffer_list->mNumberBuffers = channels;
            buffer_list->mBuffers[0] = {1, static_cast<UInt32>(channel_bytes), left_buffer.data()};
            buffer_list->mBuffers[1] = {1, static_cast<UInt32>(channel_bytes), right_buffer.data()};

            auto source = std::make_shared<OneFrameStereoSource>(
                StereoSample{static_cast<Lowl::Sample>(0.25), static_cast<Lowl::Sample>(-0.5)});
            Lowl::Audio::AudioDeviceProperties properties{};
            properties.audio_format = Lowl::Audio::AudioFormat{44100.0, Lowl::Audio::ChannelLayout::Stereo};
            properties.sample_format = sample_format;

            RecordingCoreAudioDevice device;
            device.configure(properties, source);
            REQUIRE(device.prepare(frames_per_buffer));

            AudioUnitRenderActionFlags action_flags = 0;
            AudioTimeStamp timestamp{};
            REQUIRE_EQ(device.audio_callback(&action_flags, &timestamp, 0, frames_per_buffer, buffer_list), noErr);

            if (sample_format == Lowl::Audio::SampleFormat::FLOAT_32) {
                REQUIRE_EQ(read_unaligned<float>(left_buffer.data()), doctest::Approx(0.25f));
                REQUIRE_EQ(read_unaligned<float>(right_buffer.data()), doctest::Approx(-0.5f));
                REQUIRE_EQ(read_unaligned<float>(left_buffer.data() + sizeof(float)), doctest::Approx(0.0f));
                REQUIRE_EQ(read_unaligned<float>(right_buffer.data() + sizeof(float)), doctest::Approx(0.0f));
            } else {
                REQUIRE_EQ(read_unaligned<double>(left_buffer.data()), doctest::Approx(0.25));
                REQUIRE_EQ(read_unaligned<double>(right_buffer.data()), doctest::Approx(-0.5));
                REQUIRE_EQ(read_unaligned<double>(left_buffer.data() + sizeof(double)), doctest::Approx(0.0));
                REQUIRE_EQ(read_unaligned<double>(right_buffer.data() + sizeof(double)), doctest::Approx(0.0));
            }

            for (size_t current_byte = channel_bytes; current_byte < left_buffer.size(); current_byte++) {
                REQUIRE_EQ(left_buffer[current_byte], static_cast<uint8_t>(0x7F));
                REQUIRE_EQ(right_buffer[current_byte], static_cast<uint8_t>(0x7F));
            }
        }
    }
}

#endif
