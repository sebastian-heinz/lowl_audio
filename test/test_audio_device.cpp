#include <doctest/doctest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <utility>
#include <vector>

#include "audio/backend/lowl_audio_device.h"
#include "audio/lowl_audio_buffer.h"

namespace {
    struct StereoSample {
        Lowl::Sample left;
        Lowl::Sample right;
    };

    template <typename Value> Value read_unaligned(const uint8_t *p_src) {
        Value value{};
        std::memcpy(&value, p_src, sizeof(value));
        return value;
    }

    int32_t read_int24(const uint8_t *p_src) {
        const int32_t unsigned_value = static_cast<int32_t>(p_src[0]) | (static_cast<int32_t>(p_src[1]) << 8) |
                                       (static_cast<int32_t>(p_src[2]) << 16);
        return (unsigned_value & 0x800000) != 0 ? unsigned_value - 0x1000000 : unsigned_value;
    }

    class ShortReadAudioSource final : public Lowl::Audio::AudioSource {
    public:
        explicit ShortReadAudioSource(std::vector<StereoSample> p_frames)
            : AudioSource(Lowl::Audio::AudioFormat{44100.0, Lowl::Audio::ChannelLayout::Stereo}),
              frames(std::move(p_frames)) {
        }

        RenderResult mix_into(Lowl::Audio::AudioBlockView p_block, const MixGainVector &) override {
            if (next_frame >= frames.size() || p_block.frame_count == 0) {
                return {0, RenderState::Finished};
            }

            const uint32_t frames_to_copy =
                static_cast<uint32_t>(std::min<size_t>(frames.size() - next_frame, p_block.frame_count));
            for (uint32_t frame_index = 0; frame_index < frames_to_copy; frame_index++) {
                const StereoSample &frame = frames[next_frame + frame_index];
                p_block.channel(0)[frame_index] += frame.left;
                p_block.channel(1)[frame_index] += frame.right;
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

    class OneFrameMonoSource final : public Lowl::Audio::AudioSource {
    public:
        explicit OneFrameMonoSource(const Lowl::Sample p_sample)
            : AudioSource(Lowl::Audio::AudioFormat{44100.0, Lowl::Audio::ChannelLayout::Mono}), sample(p_sample) {
        }

        RenderResult mix_into(Lowl::Audio::AudioBlockView p_block, const MixGainVector &) override {
            if (consumed || p_block.frame_count == 0) {
                return {0, RenderState::Finished};
            }
            p_block.channel(0)[0] += sample;
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
        Lowl::Sample sample = 0;
        bool consumed = false;
    };

    class ConstantMultichannelSource final : public Lowl::Audio::AudioSource {
    public:
        ConstantMultichannelSource(const Lowl::Audio::ChannelLayout p_layout, const uint32_t p_frame_count)
            : AudioSource(Lowl::Audio::AudioFormat{44100.0, p_layout}), frame_count(p_frame_count) {
        }

        RenderResult mix_into(Lowl::Audio::AudioBlockView p_block, const MixGainVector &p_upstream_gain) override {
            if (p_block.channel_count != get_channel_count()) {
                return {0, RenderState::Error};
            }

            const uint32_t frames_to_write = std::min<uint32_t>(p_block.frame_count, frame_count - frame_position);
            const MixGainVector gain = compose_gain_vector(p_upstream_gain);
            for (uint8_t channel_index = 0; channel_index < p_block.channel_count; channel_index++) {
                const Lowl::Sample value = static_cast<Lowl::Sample>(channel_index + 1) / static_cast<Lowl::Sample>(16);
                for (uint32_t frame_index = 0; frame_index < frames_to_write; frame_index++) {
                    p_block.channel(channel_index)[frame_index] += value * gain[channel_index];
                }
            }
            frame_position += frames_to_write;
            return {frames_to_write, frame_position == frame_count ? RenderState::Finished : RenderState::Ok};
        }

        Lowl::size_l get_frames_remaining() const override {
            return frame_count - frame_position;
        }

        Lowl::size_l get_frame_position() const override {
            return frame_position;
        }

        Lowl::size_l get_frame_count() const override {
            return frame_count;
        }

    private:
        uint32_t frame_count = 0;
        uint32_t frame_position = 0;
    };

    class TestAudioDevice final : public Lowl::Audio::AudioDevice {
    public:
        TestAudioDevice() : AudioDevice(_constructor_tag()) {
        }

        void configure(const Lowl::Audio::AudioDeviceProperties &p_properties,
                       std::shared_ptr<Lowl::Audio::AudioSource> p_audio_source) {
            audio_device_properties = p_properties;
            audio_source = std::move(p_audio_source);
        }

        bool prepare(unsigned long p_frames_per_buffer) {
            return allocate_render_buffer(p_frames_per_buffer);
        }

        void poison_live_configuration() {
            audio_device_properties = Lowl::Audio::AudioDeviceProperties{};
            audio_source.reset();
        }

        void clear_published_state() {
            unpublish_render_state();
        }

        bool release_retired_state() {
            return release_render_state();
        }

        bool write(void *p_dst, unsigned long p_frames_per_buffer, unsigned long p_bytes_per_frame) {
            if (!prepare(p_frames_per_buffer)) {
                return false;
            }
            write_published(p_dst,
                            static_cast<size_t>(p_frames_per_buffer) * p_bytes_per_frame,
                            p_frames_per_buffer,
                            p_bytes_per_frame);
            return true;
        }

        void write_published(void *p_dst,
                             size_t p_dst_byte_size,
                             unsigned long p_frames_per_buffer,
                             unsigned long p_bytes_per_frame) {
            render_to_device_buffer(
                load_render_state(), p_dst, p_dst_byte_size, p_frames_per_buffer, p_bytes_per_frame);
        }

        bool write_planar(void *const *p_dst_channels,
                          const size_t *p_dst_byte_sizes,
                          const uint8_t p_dst_channel_count,
                          const unsigned long p_frames_per_buffer) {
            if (!prepare(p_frames_per_buffer)) {
                return false;
            }
            return render_to_planar_device_buffers(
                load_render_state(), p_dst_channels, p_dst_byte_sizes, p_dst_channel_count, p_frames_per_buffer);
        }

        void set_properties_list(std::vector<Lowl::Audio::AudioDeviceProperties> p_properties_list) {
            properties_list = std::move(p_properties_list);
        }

        void
        start(Lowl::Audio::AudioDeviceProperties, std::shared_ptr<Lowl::Audio::AudioSource>, Lowl::Error &) override {
        }

        void stop(Lowl::Error &) override {
        }
    };
} // namespace

TEST_CASE("AudioDevice") {
    SUBCASE("AudioDeviceProperties - default initialization is safe") {
        Lowl::Audio::AudioDeviceProperties properties;
        REQUIRE_FALSE(properties.is_supported);
        REQUIRE_EQ(properties.audio_format.sample_rate, Lowl::NO_SAMPLE_RATE);
        REQUIRE_FALSE(properties.audio_format.channel_layout.is_valid());
        REQUIRE_FALSE(properties.audio_format.is_valid());
        REQUIRE_EQ(properties.sample_format, Lowl::Audio::SampleFormat::Unknown);
        REQUIRE_FALSE(properties.exclusive_mode);
        REQUIRE_EQ(properties.wasapi.valid_bits_per_sample, 0);
    }

    SUBCASE("AudioDevice - get_closest_properties prefers exact structural matches") {
        TestAudioDevice device;

        Lowl::Audio::AudioDeviceProperties exact{};
        exact.is_supported = true;
        exact.audio_format = Lowl::Audio::AudioFormat{48000.0, Lowl::Audio::ChannelLayout::Stereo};
        exact.sample_format = Lowl::Audio::SampleFormat::FLOAT_32;
        exact.exclusive_mode = false;

        Lowl::Audio::AudioDeviceProperties wrong_channel = exact;
        wrong_channel.audio_format.channel_layout = Lowl::Audio::ChannelLayout::Mono;

        Lowl::Audio::AudioDeviceProperties wrong_format = exact;
        wrong_format.sample_format = Lowl::Audio::SampleFormat::INT_16;

        device.set_properties_list({wrong_channel, wrong_format, exact});

        Lowl::Error error;
        const Lowl::Audio::AudioDeviceProperties selected = device.get_closest_properties(exact, error);
        REQUIRE_FALSE(error.has_error());
        REQUIRE_EQ(selected, exact);
    }

    SUBCASE("AudioDevice - get_closest_properties preserves WASAPI valid-bit precision") {
        TestAudioDevice device;

        Lowl::Audio::AudioDeviceProperties requested{};
        requested.is_supported = true;
        requested.audio_format = Lowl::Audio::AudioFormat{48000.0, Lowl::Audio::ChannelLayout::Stereo};
        requested.sample_format = Lowl::Audio::SampleFormat::INT_32;
        requested.wasapi.valid_bits_per_sample = 24;

        Lowl::Audio::AudioDeviceProperties full_precision = requested;
        full_precision.wasapi.valid_bits_per_sample = 32;
        device.set_properties_list({full_precision, requested});

        Lowl::Error error;
        const Lowl::Audio::AudioDeviceProperties selected = device.get_closest_properties(requested, error);
        REQUIRE_FALSE(error.has_error());
        REQUIRE_EQ(selected, requested);
        REQUIRE_NE(full_precision, requested);
        REQUIRE((full_precision < requested || requested < full_precision));
    }

    SUBCASE("AudioDevice - get_closest_properties falls back to nearest sample rate") {
        TestAudioDevice device;

        Lowl::Audio::AudioDeviceProperties requested{};
        requested.is_supported = true;
        requested.audio_format = Lowl::Audio::AudioFormat{50000.0, Lowl::Audio::ChannelLayout::Stereo};
        requested.sample_format = Lowl::Audio::SampleFormat::FLOAT_32;

        Lowl::Audio::AudioDeviceProperties low = requested;
        low.audio_format.sample_rate = 48000.0;

        Lowl::Audio::AudioDeviceProperties high = requested;
        high.audio_format.sample_rate = 96000.0;

        device.set_properties_list({high, low});

        Lowl::Error error;
        const Lowl::Audio::AudioDeviceProperties selected = device.get_closest_properties(requested, error);
        REQUIRE_FALSE(error.has_error());
        REQUIRE_EQ(selected.audio_format.sample_rate, doctest::Approx(48000.0));
    }

    SUBCASE("AudioDeviceProperties - ordering treats rounded-equal sample rates as equal") {
        Lowl::Audio::AudioDeviceProperties lhs{};
        lhs.is_supported = true;
        lhs.audio_format = Lowl::Audio::AudioFormat{48000.1, Lowl::Audio::ChannelLayout::Stereo};
        lhs.sample_format = Lowl::Audio::SampleFormat::FLOAT_32;

        Lowl::Audio::AudioDeviceProperties rhs = lhs;
        rhs.audio_format.sample_rate = 48000.4;

        REQUIRE((lhs.get_audio_format() == Lowl::Audio::AudioFormat{48000.0, Lowl::Audio::ChannelLayout::Stereo}));
        REQUIRE(lhs == rhs);
        REQUIRE_FALSE(lhs < rhs);
        REQUIRE_FALSE(rhs < lhs);
    }

    SUBCASE("AudioDevice - write_frames only zero-fills missing frames") {
        constexpr unsigned long frames_per_buffer = 3;
        constexpr unsigned long channels = 2;
        constexpr unsigned long bytes_per_frame = sizeof(float) * channels;
        constexpr size_t audio_bytes = frames_per_buffer * bytes_per_frame;
        constexpr size_t guard_bytes = 16;

        alignas(float) std::array<uint8_t, audio_bytes + guard_bytes> buffer{};
        buffer.fill(0x7F);

        auto source = std::make_shared<ShortReadAudioSource>(std::vector<StereoSample>{StereoSample{0.25f, -0.25f}});

        Lowl::Audio::AudioDeviceProperties properties{};
        properties.audio_format = Lowl::Audio::AudioFormat{44100.0, Lowl::Audio::ChannelLayout::Stereo};
        properties.sample_format = Lowl::Audio::SampleFormat::FLOAT_32;

        TestAudioDevice device;
        device.configure(properties, source);
        REQUIRE(device.write(buffer.data(), frames_per_buffer, bytes_per_frame));

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

    SUBCASE("AudioDevice - unsupported output format falls back to silence") {
        constexpr unsigned long frames_per_buffer = 2;
        constexpr unsigned long channels = 2;
        constexpr unsigned long bytes_per_frame = sizeof(float) * channels;
        constexpr size_t audio_bytes = frames_per_buffer * bytes_per_frame;
        constexpr size_t guard_bytes = 16;

        alignas(float) std::array<uint8_t, audio_bytes + guard_bytes> buffer{};
        buffer.fill(0x7F);

        auto source = std::make_shared<ShortReadAudioSource>(
            std::vector<StereoSample>{StereoSample{0.25f, -0.25f}, StereoSample{0.5f, -0.5f}});

        Lowl::Audio::AudioDeviceProperties properties{};
        properties.audio_format = Lowl::Audio::AudioFormat{44100.0, Lowl::Audio::ChannelLayout::Stereo};
        properties.sample_format = Lowl::Audio::SampleFormat::Unknown;

        TestAudioDevice device;
        device.configure(properties, source);
        REQUIRE(device.write(buffer.data(), frames_per_buffer, bytes_per_frame));

        for (size_t current_byte = 0; current_byte < audio_bytes; current_byte++) {
            REQUIRE_EQ(buffer[current_byte], static_cast<uint8_t>(0x00));
        }

        for (size_t current_byte = audio_bytes; current_byte < buffer.size(); current_byte++) {
            REQUIRE_EQ(buffer[current_byte], static_cast<uint8_t>(0x7F));
        }
    }

    SUBCASE("AudioDevice - INT16 stereo output converts and interleaves the mixed block") {
        constexpr unsigned long frames_per_buffer = 2;
        constexpr unsigned long channels = 2;
        constexpr unsigned long bytes_per_frame = sizeof(int16_t) * channels;

        std::array<int16_t, frames_per_buffer * channels> buffer{};
        buffer.fill(static_cast<int16_t>(0x1234));

        auto source = std::make_shared<ShortReadAudioSource>(std::vector<StereoSample>{
            StereoSample{0.25f, -0.25f},
            StereoSample{-1.0f, 1.0f},
        });

        Lowl::Audio::AudioDeviceProperties properties{};
        properties.audio_format = Lowl::Audio::AudioFormat{44100.0, Lowl::Audio::ChannelLayout::Stereo};
        properties.sample_format = Lowl::Audio::SampleFormat::INT_16;

        TestAudioDevice device;
        device.configure(properties, source);
        REQUIRE(device.write(buffer.data(), frames_per_buffer, bytes_per_frame));

        REQUIRE_EQ(buffer[0], static_cast<int16_t>(8191));
        REQUIRE_EQ(buffer[1], static_cast<int16_t>(-8191));
        REQUIRE_EQ(buffer[2], static_cast<int16_t>(-32767));
        REQUIRE_EQ(buffer[3], static_cast<int16_t>(32767));
    }

    SUBCASE("AudioDevice - every output SampleFormat converts and interleaves Sample channels") {
        constexpr unsigned long frames_per_buffer = 2;
        constexpr uint8_t channels = 2;
        constexpr size_t max_audio_bytes = frames_per_buffer * channels * sizeof(double);
        constexpr size_t guard_bytes = 16;
        const std::array<Lowl::Audio::SampleFormat, 7> sample_formats = {
            Lowl::Audio::SampleFormat::FLOAT_32,
            Lowl::Audio::SampleFormat::FLOAT_64,
            Lowl::Audio::SampleFormat::INT_32,
            Lowl::Audio::SampleFormat::INT_24,
            Lowl::Audio::SampleFormat::INT_16,
            Lowl::Audio::SampleFormat::INT_8,
            Lowl::Audio::SampleFormat::U_INT_8,
        };

        for (const Lowl::Audio::SampleFormat sample_format : sample_formats) {
            alignas(double) std::array<uint8_t, max_audio_bytes + guard_bytes> buffer{};
            buffer.fill(0x7F);

            auto source = std::make_shared<ShortReadAudioSource>(std::vector<StereoSample>{
                StereoSample{static_cast<Lowl::Sample>(0.25), static_cast<Lowl::Sample>(-0.5)}});

            Lowl::Audio::AudioDeviceProperties properties{};
            properties.audio_format = Lowl::Audio::AudioFormat{44100.0, Lowl::Audio::ChannelLayout::Stereo};
            properties.sample_format = sample_format;

            const size_t sample_size = Lowl::Audio::get_sample_size_bytes(sample_format);
            const unsigned long bytes_per_frame = static_cast<unsigned long>(sample_size * channels);
            const size_t audio_bytes = frames_per_buffer * bytes_per_frame;

            TestAudioDevice device;
            device.configure(properties, source);
            REQUIRE(device.write(buffer.data(), frames_per_buffer, bytes_per_frame));

            switch (sample_format) {
                case Lowl::Audio::SampleFormat::FLOAT_32:
                    REQUIRE_EQ(read_unaligned<float>(buffer.data()), doctest::Approx(0.25f));
                    REQUIRE_EQ(read_unaligned<float>(buffer.data() + sizeof(float)), doctest::Approx(-0.5f));
                    break;
                case Lowl::Audio::SampleFormat::FLOAT_64:
                    REQUIRE_EQ(read_unaligned<double>(buffer.data()), doctest::Approx(0.25));
                    REQUIRE_EQ(read_unaligned<double>(buffer.data() + sizeof(double)), doctest::Approx(-0.5));
                    break;
                case Lowl::Audio::SampleFormat::INT_32:
                    REQUIRE_EQ(read_unaligned<int32_t>(buffer.data()), 536870911);
                    REQUIRE_EQ(read_unaligned<int32_t>(buffer.data() + sizeof(int32_t)), -1073741823);
                    break;
                case Lowl::Audio::SampleFormat::INT_24:
                    REQUIRE_EQ(read_int24(buffer.data()), 2097152);
                    REQUIRE_EQ(read_int24(buffer.data() + 3), -4194304);
                    break;
                case Lowl::Audio::SampleFormat::INT_16:
                    REQUIRE_EQ(read_unaligned<int16_t>(buffer.data()), static_cast<int16_t>(8191));
                    REQUIRE_EQ(read_unaligned<int16_t>(buffer.data() + sizeof(int16_t)), static_cast<int16_t>(-16383));
                    break;
                case Lowl::Audio::SampleFormat::INT_8:
                    REQUIRE_EQ(read_unaligned<int8_t>(buffer.data()), static_cast<int8_t>(31));
                    REQUIRE_EQ(read_unaligned<int8_t>(buffer.data() + sizeof(int8_t)), static_cast<int8_t>(-63));
                    break;
                case Lowl::Audio::SampleFormat::U_INT_8:
                    REQUIRE_EQ(buffer[0], static_cast<uint8_t>(159));
                    REQUIRE_EQ(buffer[1], static_cast<uint8_t>(65));
                    break;
                case Lowl::Audio::SampleFormat::Unknown:
                    REQUIRE(false);
                    break;
            }

            const uint8_t silence = sample_format == Lowl::Audio::SampleFormat::U_INT_8 ? 0x80 : 0x00;
            for (size_t current_byte = bytes_per_frame; current_byte < audio_bytes; current_byte++) {
                REQUIRE_EQ(buffer[current_byte], silence);
            }
            for (size_t current_byte = audio_bytes; current_byte < buffer.size(); current_byte++) {
                REQUIRE_EQ(buffer[current_byte], static_cast<uint8_t>(0x7F));
            }
        }
    }

    SUBCASE("AudioDevice - 24 valid PCM bits are left-aligned in a 32-bit WASAPI container") {
        constexpr unsigned long frames_per_buffer = 2;
        constexpr unsigned long channels = 2;
        constexpr unsigned long bytes_per_frame = sizeof(int32_t) * channels;
        constexpr int32_t guard_value = 0x12345678;

        std::array<int32_t, frames_per_buffer * channels + 2> buffer{};
        buffer.fill(guard_value);

        auto source = std::make_shared<ShortReadAudioSource>(std::vector<StereoSample>{
            StereoSample{static_cast<Lowl::Sample>(0.25), static_cast<Lowl::Sample>(-0.5)},
        });
        Lowl::Audio::AudioDeviceProperties properties{};
        properties.audio_format = Lowl::Audio::AudioFormat{44100.0, Lowl::Audio::ChannelLayout::Stereo};
        properties.sample_format = Lowl::Audio::SampleFormat::INT_32;
        properties.wasapi.valid_bits_per_sample = 24;

        TestAudioDevice device;
        device.configure(properties, source);
        REQUIRE(device.write(buffer.data(), frames_per_buffer, bytes_per_frame));

        REQUIRE_EQ(buffer[0], static_cast<int32_t>(2097152 * 256));
        REQUIRE_EQ(buffer[1], static_cast<int32_t>(-4194304 * 256));
        REQUIRE_EQ(buffer[2], 0);
        REQUIRE_EQ(buffer[3], 0);
        REQUIRE_EQ(buffer[4], guard_value);
        REQUIRE_EQ(buffer[5], guard_value);
        REQUIRE_EQ(static_cast<uint32_t>(buffer[0]) & 0xFFU, 0U);
        REQUIRE_EQ(static_cast<uint32_t>(buffer[1]) & 0xFFU, 0U);
    }

    SUBCASE("AudioDevice - FLOAT32 mono output uses the Sample-width-correct path") {
        constexpr unsigned long frames_per_buffer = 2;
        constexpr unsigned long bytes_per_frame = sizeof(float);
        constexpr size_t audio_bytes = frames_per_buffer * bytes_per_frame;
        constexpr size_t guard_bytes = 16;
        alignas(float) std::array<uint8_t, audio_bytes + guard_bytes> buffer{};
        buffer.fill(0x7F);

        auto source = std::make_shared<OneFrameMonoSource>(static_cast<Lowl::Sample>(0.25));
        Lowl::Audio::AudioDeviceProperties properties{};
        properties.audio_format = Lowl::Audio::AudioFormat{44100.0, Lowl::Audio::ChannelLayout::Mono};
        properties.sample_format = Lowl::Audio::SampleFormat::FLOAT_32;

        TestAudioDevice device;
        device.configure(properties, source);
        REQUIRE(device.write(buffer.data(), frames_per_buffer, bytes_per_frame));

        REQUIRE_EQ(read_unaligned<float>(buffer.data()), doctest::Approx(0.25f));
        REQUIRE_EQ(read_unaligned<float>(buffer.data() + sizeof(float)), doctest::Approx(0.0f));
        for (size_t current_byte = audio_bytes; current_byte < buffer.size(); current_byte++) {
            REQUIRE_EQ(buffer[current_byte], static_cast<uint8_t>(0x7F));
        }
    }

    SUBCASE("AudioDevice - planar floating output converts Sample channels without overruns") {
        constexpr unsigned long frames_per_buffer = 2;
        constexpr uint8_t channels = 2;
        constexpr size_t max_channel_bytes = frames_per_buffer * sizeof(double);
        constexpr size_t guard_bytes = 16;
        const std::array<Lowl::Audio::SampleFormat, 2> sample_formats = {
            Lowl::Audio::SampleFormat::FLOAT_32,
            Lowl::Audio::SampleFormat::FLOAT_64,
        };

        for (const Lowl::Audio::SampleFormat sample_format : sample_formats) {
            alignas(double) std::array<uint8_t, max_channel_bytes + guard_bytes> left_buffer{};
            alignas(double) std::array<uint8_t, max_channel_bytes + guard_bytes> right_buffer{};
            left_buffer.fill(0x7F);
            right_buffer.fill(0x7F);

            auto source = std::make_shared<ShortReadAudioSource>(std::vector<StereoSample>{
                StereoSample{static_cast<Lowl::Sample>(0.25), static_cast<Lowl::Sample>(-0.5)}});

            Lowl::Audio::AudioDeviceProperties properties{};
            properties.audio_format = Lowl::Audio::AudioFormat{44100.0, Lowl::Audio::ChannelLayout::Stereo};
            properties.sample_format = sample_format;

            const size_t sample_size = Lowl::Audio::get_sample_size_bytes(sample_format);
            const size_t channel_bytes = frames_per_buffer * sample_size;
            std::array<void *, channels> dst_channels = {left_buffer.data(), right_buffer.data()};
            std::array<size_t, channels> dst_byte_sizes = {channel_bytes, channel_bytes};

            TestAudioDevice device;
            device.configure(properties, source);
            REQUIRE(device.write_planar(dst_channels.data(), dst_byte_sizes.data(), channels, frames_per_buffer));

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

    SUBCASE("AudioDevice - large callbacks interleave every channel layout from one through eight") {
        constexpr uint32_t frames_per_buffer = 4096;
        constexpr size_t guard_samples = 16;
        constexpr float guard_value = 123.0f;

        for (uint8_t channel_count = 1; channel_count <= Lowl::Audio::ChannelLayout::MaxChannels; channel_count++) {
            CAPTURE(channel_count);
            const Lowl::Audio::ChannelLayout layout = Lowl::Audio::ChannelLayout::from_count(channel_count);
            REQUIRE(layout.is_valid());

            auto source = std::make_shared<ConstantMultichannelSource>(layout, frames_per_buffer);
            Lowl::Audio::AudioDeviceProperties properties{};
            properties.audio_format = Lowl::Audio::AudioFormat{44100.0, layout};
            properties.sample_format = Lowl::Audio::SampleFormat::FLOAT_32;

            const size_t audio_sample_count = static_cast<size_t>(frames_per_buffer) * channel_count;
            std::vector<float> buffer(audio_sample_count + guard_samples, guard_value);

            TestAudioDevice device;
            device.configure(properties, source);
            REQUIRE(device.write(
                buffer.data(), frames_per_buffer, static_cast<unsigned long>(sizeof(float) * channel_count)));

            const uint32_t sampled_frames[] = {
                0,
                frames_per_buffer / 2,
                frames_per_buffer - 1,
            };
            for (const uint32_t frame_index : sampled_frames) {
                for (uint8_t channel_index = 0; channel_index < channel_count; channel_index++) {
                    const float expected = static_cast<float>(channel_index + 1) / 16.0f;
                    REQUIRE_EQ(buffer[static_cast<size_t>(frame_index) * channel_count + channel_index],
                               doctest::Approx(expected));
                }
            }
            for (size_t guard_index = audio_sample_count; guard_index < buffer.size(); guard_index++) {
                REQUIRE_EQ(buffer[guard_index], doctest::Approx(guard_value));
            }
        }
    }

    SUBCASE("AudioDevice - published render state is independent from live configuration") {
        constexpr unsigned long frames_per_buffer = 2;
        constexpr unsigned long channels = 2;
        constexpr unsigned long bytes_per_frame = sizeof(float) * channels;
        constexpr size_t audio_bytes = frames_per_buffer * bytes_per_frame;

        alignas(float) std::array<uint8_t, audio_bytes> buffer{};
        buffer.fill(0);

        auto source = std::make_shared<ShortReadAudioSource>(
            std::vector<StereoSample>{StereoSample{0.25f, -0.25f}, StereoSample{0.5f, -0.5f}});

        Lowl::Audio::AudioDeviceProperties properties{};
        properties.audio_format = Lowl::Audio::AudioFormat{44100.0, Lowl::Audio::ChannelLayout::Stereo};
        properties.sample_format = Lowl::Audio::SampleFormat::FLOAT_32;

        TestAudioDevice device;
        device.configure(properties, source);
        REQUIRE(device.prepare(frames_per_buffer));
        device.poison_live_configuration();
        device.write_published(buffer.data(), audio_bytes, frames_per_buffer, bytes_per_frame);

        auto *samples = reinterpret_cast<const float *>(buffer.data());
        REQUIRE_EQ(samples[0], doctest::Approx(0.25f));
        REQUIRE_EQ(samples[1], doctest::Approx(-0.25f));
        REQUIRE_EQ(samples[2], doctest::Approx(0.5f));
        REQUIRE_EQ(samples[3], doctest::Approx(-0.5f));
    }

    SUBCASE("AudioDevice - cleared published render state produces silence") {
        constexpr unsigned long frames_per_buffer = 2;
        constexpr unsigned long channels = 2;
        constexpr unsigned long bytes_per_frame = sizeof(float) * channels;
        constexpr size_t audio_bytes = frames_per_buffer * bytes_per_frame;

        alignas(float) std::array<uint8_t, audio_bytes> buffer{};
        buffer.fill(0x7F);

        auto source = std::make_shared<ShortReadAudioSource>(
            std::vector<StereoSample>{StereoSample{0.25f, -0.25f}, StereoSample{0.5f, -0.5f}});

        Lowl::Audio::AudioDeviceProperties properties{};
        properties.audio_format = Lowl::Audio::AudioFormat{44100.0, Lowl::Audio::ChannelLayout::Stereo};
        properties.sample_format = Lowl::Audio::SampleFormat::FLOAT_32;

        TestAudioDevice device;
        device.configure(properties, source);
        REQUIRE(device.prepare(frames_per_buffer));
        device.clear_published_state();
        device.write_published(buffer.data(), audio_bytes, frames_per_buffer, bytes_per_frame);

        auto *samples = reinterpret_cast<const float *>(buffer.data());
        for (size_t current_sample = 0; current_sample < frames_per_buffer * channels; current_sample++) {
            REQUIRE_EQ(samples[current_sample], doctest::Approx(0.0f));
        }
    }

    SUBCASE("AudioDevice - unpublished render state remains alive until explicit retirement") {
        constexpr unsigned long frames_per_buffer = 2;

        auto source = std::make_shared<ShortReadAudioSource>(std::vector<StereoSample>{StereoSample{0.25f, -0.25f}});
        std::weak_ptr<ShortReadAudioSource> source_lifetime = source;

        Lowl::Audio::AudioDeviceProperties properties{};
        properties.audio_format = Lowl::Audio::AudioFormat{44100.0, Lowl::Audio::ChannelLayout::Stereo};
        properties.sample_format = Lowl::Audio::SampleFormat::FLOAT_32;

        TestAudioDevice device;
        device.configure(properties, source);
        REQUIRE(device.prepare(frames_per_buffer));

        source.reset();
        device.poison_live_configuration();
        device.clear_published_state();

        REQUIRE_FALSE(source_lifetime.expired());

        REQUIRE(device.release_retired_state());

        REQUIRE(source_lifetime.expired());
    }

    SUBCASE("AudioDevice - render_to_device_buffer does not overrun when caller buffer is smaller than requested") {
        constexpr unsigned long frames_per_buffer = 2;
        constexpr unsigned long channels = 2;
        constexpr unsigned long bytes_per_frame = sizeof(float) * channels;
        constexpr size_t safe_audio_bytes = bytes_per_frame;
        constexpr size_t guard_bytes = 16;

        alignas(float) std::array<uint8_t, safe_audio_bytes + guard_bytes> buffer{};
        buffer.fill(0x7F);

        auto source = std::make_shared<ShortReadAudioSource>(
            std::vector<StereoSample>{StereoSample{0.25f, -0.25f}, StereoSample{0.5f, -0.5f}});

        Lowl::Audio::AudioDeviceProperties properties{};
        properties.audio_format = Lowl::Audio::AudioFormat{44100.0, Lowl::Audio::ChannelLayout::Stereo};
        properties.sample_format = Lowl::Audio::SampleFormat::FLOAT_32;

        TestAudioDevice device;
        device.configure(properties, source);
        REQUIRE(device.prepare(frames_per_buffer));
        device.write_published(buffer.data(), safe_audio_bytes, frames_per_buffer, bytes_per_frame);

        for (size_t current_byte = 0; current_byte < safe_audio_bytes; current_byte++) {
            REQUIRE_EQ(buffer[current_byte], static_cast<uint8_t>(0x00));
        }

        for (size_t current_byte = safe_audio_bytes; current_byte < buffer.size(); current_byte++) {
            REQUIRE_EQ(buffer[current_byte], static_cast<uint8_t>(0x7F));
        }
    }
}
