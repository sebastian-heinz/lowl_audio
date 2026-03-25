#ifndef LOWL_AUDIO_BUFFER_H
#define LOWL_AUDIO_BUFFER_H

#include "lowl_typedef.h"

#include <array>
#include <memory>

namespace Lowl::Audio {
    struct AudioBlockView {
        static constexpr uint32_t MAX_CHANNELS = 8;

        std::array<Sample *, MAX_CHANNELS> channels{};
        uint32_t frame_count = 0;
        uint8_t channel_count = 0;

        _INLINE_ Sample *channel(uint8_t p_channel) {
            return channels[static_cast<size_t>(p_channel)];
        }

        _INLINE_ const Sample *channel(uint8_t p_channel) const {
            return channels[static_cast<size_t>(p_channel)];
        }
    };

    class AudioBuffer {
    private:
        std::unique_ptr<Sample[]> storage;
        std::array<Sample *, AudioBlockView::MAX_CHANNELS> channel_ptrs{};
        uint32_t frame_capacity = 0;
        uint8_t channel_count = 0;

        void rebuild_channel_ptrs();

    public:
        AudioBuffer() = default;
        AudioBuffer(uint32_t p_frame_capacity, uint8_t p_channel_count);
        AudioBuffer(AudioBuffer &&p_other) noexcept;
        AudioBuffer &operator=(AudioBuffer &&p_other) noexcept;
        AudioBuffer(const AudioBuffer &) = delete;
        AudioBuffer &operator=(const AudioBuffer &) = delete;

        uint32_t get_frame_capacity() const;
        uint8_t get_channel_count() const;

        AudioBlockView view(uint32_t p_frame_count);
        void clear(uint32_t p_frame_count);
    };
}

#endif
