#include "lowl_audio_buffer.h"

#include <algorithm>
#include <cstring>
#include <utility>

void Lowl::Audio::AudioBuffer::rebuild_channel_ptrs() {
    channel_ptrs.fill(nullptr);
    if (!storage) {
        return;
    }
    for (uint8_t channel_index = 0; channel_index < channel_count; channel_index++) {
        channel_ptrs[static_cast<size_t>(channel_index)] =
            storage.get() + static_cast<size_t>(channel_index) * frame_stride;
    }
}

Lowl::Audio::AudioBuffer::AudioBuffer(const uint32_t p_frame_capacity, const uint8_t p_channel_count) {
    frame_capacity = p_frame_capacity;
    frame_stride = static_cast<uint32_t>(aligned_frame_stride(p_frame_capacity));
    channel_count = std::min<uint8_t>(p_channel_count, AudioBlockView::MAX_CHANNELS);
    if (frame_capacity > 0 && channel_count > 0) {
        storage = allocate_aligned_samples(static_cast<size_t>(frame_stride) * channel_count);
        rebuild_channel_ptrs();
    }
}

Lowl::Audio::AudioBuffer::AudioBuffer(AudioBuffer &&p_other) noexcept
    : storage(std::move(p_other.storage)), frame_capacity(p_other.frame_capacity), frame_stride(p_other.frame_stride),
      channel_count(p_other.channel_count) {
    rebuild_channel_ptrs();
    p_other.channel_ptrs = {};
    p_other.frame_capacity = 0;
    p_other.frame_stride = 0;
    p_other.channel_count = 0;
}

Lowl::Audio::AudioBuffer &Lowl::Audio::AudioBuffer::operator=(AudioBuffer &&p_other) noexcept {
    if (this == &p_other) {
        return *this;
    }
    storage = std::move(p_other.storage);
    frame_capacity = p_other.frame_capacity;
    frame_stride = p_other.frame_stride;
    channel_count = p_other.channel_count;
    rebuild_channel_ptrs();
    p_other.channel_ptrs = {};
    p_other.frame_capacity = 0;
    p_other.frame_stride = 0;
    p_other.channel_count = 0;
    return *this;
}

Lowl::uint32_l Lowl::Audio::AudioBuffer::get_frame_capacity() const {
    return frame_capacity;
}

Lowl::uint32_l Lowl::Audio::AudioBuffer::get_frame_stride() const {
    return frame_stride;
}

uint8_t Lowl::Audio::AudioBuffer::get_channel_count() const {
    return channel_count;
}

Lowl::Audio::AudioBlockView Lowl::Audio::AudioBuffer::view(const uint32_t p_frame_count) {
    AudioBlockView block_view{};
    block_view.frame_count = std::min(p_frame_count, frame_capacity);
    block_view.channel_count = channel_count;
    for (uint8_t channel_index = 0; channel_index < channel_count; channel_index++) {
        block_view.channels[static_cast<size_t>(channel_index)] = channel_ptrs[static_cast<size_t>(channel_index)];
    }
    return block_view;
}

void Lowl::Audio::AudioBuffer::clear(const uint32_t p_frame_count) {
    const uint32_t frames_to_clear = std::min(p_frame_count, frame_capacity);
    const size_t bytes_to_clear = static_cast<size_t>(frames_to_clear) * sizeof(Sample);
    for (uint8_t channel_index = 0; channel_index < channel_count; channel_index++) {
        Sample *channel = channel_ptrs[static_cast<size_t>(channel_index)];
        if (channel != nullptr) {
            std::memset(channel, 0, bytes_to_clear);
        }
    }
}
