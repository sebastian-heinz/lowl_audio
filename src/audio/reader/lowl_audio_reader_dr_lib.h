#ifndef LOWL_AUDIO_READER_DR_LIB_H
#define LOWL_AUDIO_READER_DR_LIB_H

#include <cstddef>
#include <cstdint>
#include <memory>

namespace Lowl::Audio::DrLib {
    enum class WavFormatTag : uint32_t {
        Unknown = 0,
        Pcm = 1,
        Adpcm = 2,
        IeeeFloat = 3,
        Alaw = 6,
        Mulaw = 7,
        DviAdpcm = 0x11,
    };

    struct WavInfo {
        uint64_t total_pcm_frame_count = 0;
        uint32_t bytes_per_pcm_frame = 0;
        WavFormatTag format_tag = WavFormatTag::Unknown;
        uint32_t sample_rate = 0;
        uint32_t channel_mask = 0;
        uint16_t channels = 0;
        uint16_t bits_per_sample = 0;
    };

    class WavDecoder final {
    private:
        struct Impl;
        std::unique_ptr<Impl> impl;

    public:
        WavDecoder(const uint8_t *p_data, size_t p_size);
        ~WavDecoder();

        WavDecoder(WavDecoder &&p_other) noexcept;
        WavDecoder &operator=(WavDecoder &&p_other) noexcept;

        WavDecoder(const WavDecoder &) = delete;
        WavDecoder &operator=(const WavDecoder &) = delete;

        bool is_open() const;
        WavInfo get_info() const;
        size_t read_raw(size_t p_bytes_to_read, void *p_dst);
    };

    struct FlacInfo {
        uint64_t total_pcm_frame_count = 0;
        uint32_t sample_rate = 0;
        uint8_t channels = 0;
    };

    class FlacDecoder final {
    private:
        struct Impl;
        std::unique_ptr<Impl> impl;

    public:
        FlacDecoder(const uint8_t *p_data, size_t p_size);
        ~FlacDecoder();

        FlacDecoder(FlacDecoder &&p_other) noexcept;
        FlacDecoder &operator=(FlacDecoder &&p_other) noexcept;

        FlacDecoder(const FlacDecoder &) = delete;
        FlacDecoder &operator=(const FlacDecoder &) = delete;

        bool is_open() const;
        FlacInfo get_info() const;
        size_t read_pcm_frames_s32(size_t p_frames_to_read, int32_t *p_buffer_out);
    };

    struct Mp3Info {
        uint64_t total_pcm_frame_count = 0;
        uint32_t sample_rate = 0;
        uint8_t channels = 0;
    };

    class Mp3Decoder final {
    private:
        struct Impl;
        std::unique_ptr<Impl> impl;

    public:
        Mp3Decoder(const uint8_t *p_data, size_t p_size);
        ~Mp3Decoder();

        Mp3Decoder(Mp3Decoder &&p_other) noexcept;
        Mp3Decoder &operator=(Mp3Decoder &&p_other) noexcept;

        Mp3Decoder(const Mp3Decoder &) = delete;
        Mp3Decoder &operator=(const Mp3Decoder &) = delete;

        bool is_open() const;
        Mp3Info get_info() const;
        uint64_t read_pcm_frames_f32(uint64_t p_frames_to_read, float *p_buffer_out);
    };
} // namespace Lowl::Audio::DrLib

#endif
