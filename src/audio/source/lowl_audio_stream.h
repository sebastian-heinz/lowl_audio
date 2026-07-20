#ifndef LOWL_AUDIO_STREAM_H
#define LOWL_AUDIO_STREAM_H

#include <atomic>
#include <vector>

#include "audio/source/lowl_audio_source.h"

namespace Lowl::Audio {
    /**
     * Ring-buffer backed source for streaming sample blocks into the audio pipeline.
     */
    class AudioStream : public AudioSource {
    public:
        static constexpr size_t DEFAULT_STREAM_SIZE = 375000; // ~ 7 Seconds(3 MB) of stereo 32-bit float audio

    private:
        static constexpr size_t CacheLineSize = 64;

        struct alignas(CacheLineSize) ProducerState {
            std::atomic<size_t> write_position{0};
            size_t cached_read_position = 0;
        };

        struct alignas(CacheLineSize) ConsumerState {
            std::atomic<size_t> read_position{0};
            size_t cached_write_position = 0;
        };

        AudioBuffer ring_buffer;
        size_t frame_capacity = 0;
        size_t storage_capacity = 0;
        size_t capacity_mask = 0;
        ProducerState producer_state;
        ConsumerState consumer_state;

        size_t get_available_frames_to_read() const;
        size_t get_readable_frames(size_t p_current_read, size_t p_requested_frames);
        size_t get_writable_frames(size_t p_current_write, size_t p_requested_frames);
        void copy_from_ring(AudioBlockView p_block, uint32_t p_frames_to_read, size_t p_read_position);
        void copy_interleaved_to_ring(const Sample *p_interleaved, size_t p_frame_count, size_t p_write_position);
        void copy_planar_to_ring(const std::vector<const Sample *> &p_channels,
                                 size_t p_frame_count,
                                 size_t p_write_position);

    public:
        size_l get_frames_remaining() const override;

        size_l get_frame_position() const override;

        size_l get_frame_count() const override;

        RenderResult mix_into(AudioBlockView p_block,
                              const MixGainVector &p_upstream_gain) override;

        size_l write_interleaved(const Sample *p_interleaved, size_t p_frame_count);

        size_l write_planar(const std::vector<const Sample *> &p_channels, size_t p_frame_count);

        explicit AudioStream(AudioFormat p_audio_format, size_t size = DEFAULT_STREAM_SIZE);

        ~AudioStream() override = default;
    };
} // namespace Lowl::Audio

#endif
