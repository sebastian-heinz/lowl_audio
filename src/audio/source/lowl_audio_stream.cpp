#include "lowl_audio_stream.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace {
    size_t round_up_to_power_of_two(const size_t p_size) {
        if (p_size == 0) {
            return 0;
        }

        size_t rounded = p_size - 1;
        for (size_t shift = 1; shift < std::numeric_limits<size_t>::digits; shift <<= 1) {
            rounded |= rounded >> shift;
        }

        if (rounded == std::numeric_limits<size_t>::max()) {
            return 0;
        }

        return rounded + 1;
    }

    size_t get_readable_frame_count(const size_t p_write_position, const size_t p_read_position) {
        return p_write_position - p_read_position;
    }

    size_t get_writable_frame_count(const size_t p_frame_capacity,
                                    const size_t p_write_position,
                                    const size_t p_read_position) {
        const size_t used_frames = get_readable_frame_count(p_write_position, p_read_position);
        return used_frames >= p_frame_capacity ? 0 : p_frame_capacity - used_frames;
    }
} // namespace

Lowl::Audio::AudioStream::AudioStream(const AudioFormat p_audio_format, const size_t size)
    : AudioSource(p_audio_format) {
    frame_capacity = size;
    storage_capacity = round_up_to_power_of_two(size);
    if (frame_capacity > 0 && storage_capacity == 0) {
        throw std::length_error("AudioStream capacity is too large to round to a power of two");
    }
    capacity_mask = storage_capacity == 0 ? 0 : storage_capacity - 1;
    const uint8_t channel_count = get_channel_count();
    ring_buffer = AudioBuffer(static_cast<uint32_t>(storage_capacity), channel_count);
}

size_t Lowl::Audio::AudioStream::get_available_frames_to_read() const {
    const size_t current_write = producer_state.write_position.load(std::memory_order_acquire);
    const size_t current_read = consumer_state.read_position.load(std::memory_order_relaxed);
    return get_readable_frame_count(current_write, current_read);
}

size_t Lowl::Audio::AudioStream::get_readable_frames(const size_t p_current_read, const size_t p_requested_frames) {
    size_t readable_frames = get_readable_frame_count(consumer_state.cached_write_position, p_current_read);
    if (readable_frames < p_requested_frames) {
        consumer_state.cached_write_position = producer_state.write_position.load(std::memory_order_acquire);
        readable_frames = get_readable_frame_count(consumer_state.cached_write_position, p_current_read);
    }
    return readable_frames;
}

size_t Lowl::Audio::AudioStream::get_writable_frames(const size_t p_current_write, const size_t p_requested_frames) {
    size_t writable_frames =
        get_writable_frame_count(frame_capacity, p_current_write, producer_state.cached_read_position);
    if (writable_frames < p_requested_frames) {
        producer_state.cached_read_position = consumer_state.read_position.load(std::memory_order_acquire);
        writable_frames =
            get_writable_frame_count(frame_capacity, p_current_write, producer_state.cached_read_position);
    }
    return writable_frames;
}

void Lowl::Audio::AudioStream::copy_from_ring(AudioBlockView p_block,
                                              const uint32_t p_frames_to_read,
                                              const size_t p_read_position) {
    if (storage_capacity == 0 || p_frames_to_read == 0) {
        return;
    }

    const size_t first_index = p_read_position & capacity_mask;
    const uint32_t first_part_frames =
        static_cast<uint32_t>(std::min<size_t>(p_frames_to_read, storage_capacity - first_index));
    const uint32_t second_part_frames = p_frames_to_read - first_part_frames;
    AudioBlockView ring_view = ring_buffer.view(static_cast<uint32_t>(storage_capacity));

    for (uint8_t channel_index = 0; channel_index < p_block.channel_count; channel_index++) {
        Sample *dst = p_block.channel(channel_index);
        const Sample *src_channel = ring_view.channel(channel_index);
        std::copy_n(src_channel + first_index, first_part_frames, dst);
        if (second_part_frames > 0) {
            std::copy_n(src_channel, second_part_frames, dst + first_part_frames);
        }
    }
}

void Lowl::Audio::AudioStream::copy_interleaved_to_ring(const Sample *p_interleaved,
                                                        const size_t p_frame_count,
                                                        const size_t p_write_position) {
    if (storage_capacity == 0 || p_interleaved == nullptr || p_frame_count == 0) {
        return;
    }

    const size_t channel_count = get_channel_count();
    const size_t first_index = p_write_position & capacity_mask;
    const size_t first_part_frames = std::min(p_frame_count, storage_capacity - first_index);
    const size_t second_part_frames = p_frame_count - first_part_frames;
    AudioBlockView ring_view = ring_buffer.view(static_cast<uint32_t>(storage_capacity));

    for (size_t channel_index = 0; channel_index < channel_count; channel_index++) {
        Sample *dst = ring_view.channel(static_cast<uint8_t>(channel_index)) + first_index;
        const Sample *src = p_interleaved + channel_index;

        for (size_t frame_index = 0; frame_index < first_part_frames; frame_index++) {
            *dst++ = *src;
            src += channel_count;
        }

        dst = ring_view.channel(static_cast<uint8_t>(channel_index));
        for (size_t frame_index = 0; frame_index < second_part_frames; frame_index++) {
            *dst++ = *src;
            src += channel_count;
        }
    }
}

void Lowl::Audio::AudioStream::copy_planar_to_ring(const std::vector<const Sample *> &p_channels,
                                                   const size_t p_frame_count,
                                                   const size_t p_write_position) {
    if (storage_capacity == 0 || p_frame_count == 0) {
        return;
    }

    const size_t channel_count = get_channel_count();
    const size_t first_index = p_write_position & capacity_mask;
    const size_t first_part_frames = std::min(p_frame_count, storage_capacity - first_index);
    const size_t second_part_frames = p_frame_count - first_part_frames;
    AudioBlockView ring_view = ring_buffer.view(static_cast<uint32_t>(storage_capacity));

    for (size_t channel_index = 0; channel_index < channel_count; channel_index++) {
        const Sample *src_channel = channel_index < p_channels.size() ? p_channels[channel_index] : nullptr;
        Sample *dst_channel = ring_view.channel(static_cast<uint8_t>(channel_index));
        if (src_channel != nullptr) {
            std::copy_n(src_channel, first_part_frames, dst_channel + first_index);
            if (second_part_frames > 0) {
                std::copy_n(src_channel + first_part_frames, second_part_frames, dst_channel);
            }
        } else {
            std::fill_n(dst_channel + first_index, first_part_frames, static_cast<Sample>(0));
            if (second_part_frames > 0) {
                std::fill_n(dst_channel, second_part_frames, static_cast<Sample>(0));
            }
        }
    }
}

Lowl::Audio::AudioSource::RenderResult Lowl::Audio::AudioStream::mix_into(AudioBlockView p_block,
                                                                          const MixGainVector &p_upstream_gain) {
    if (!playback_enabled.load(std::memory_order_relaxed)) {
        return {0, RenderState::Starved};
    }

    const uint8_t expected_channel_count = get_channel_count();
    if (p_block.channel_count != expected_channel_count) {
        for (uint8_t channel_index = 0; channel_index < p_block.channel_count; channel_index++) {
            std::fill_n(p_block.channel(channel_index), p_block.frame_count, static_cast<Sample>(0));
        }
        return {0, RenderState::Error};
    }

    const size_t current_read = consumer_state.read_position.load(std::memory_order_relaxed);
    const uint32_t frames_to_read = static_cast<uint32_t>(
        std::min<size_t>(get_readable_frames(current_read, p_block.frame_count), p_block.frame_count));
    if (frames_to_read == 0) {
        return {0, RenderState::Starved};
    }

    const MixGainVector effective_gain = compose_gain_vector(p_upstream_gain);
    const size_t first_index = current_read & capacity_mask;
    const uint32_t first_part_frames =
        static_cast<uint32_t>(std::min<size_t>(frames_to_read, storage_capacity - first_index));
    const uint32_t second_part_frames = frames_to_read - first_part_frames;
    AudioBlockView ring_view = ring_buffer.view(static_cast<uint32_t>(storage_capacity));

    for (uint8_t channel_index = 0; channel_index < p_block.channel_count; channel_index++) {
        const Sample gain = effective_gain[channel_index];
        if (std::abs(gain) <= std::numeric_limits<Sample>::epsilon()) {
            continue;
        }

        Sample *dst = p_block.channel(channel_index);
        const Sample *src_channel = ring_view.channel(channel_index);
        mix_scaled_channel(src_channel + first_index, dst, gain, first_part_frames);
        if (second_part_frames > 0) {
            mix_scaled_channel(src_channel, dst + first_part_frames, gain, second_part_frames);
        }
    }

    consumer_state.read_position.store(current_read + frames_to_read, std::memory_order_release);

    return {frames_to_read, RenderState::Ok};
}

Lowl::size_l Lowl::Audio::AudioStream::write_interleaved(const Sample *p_interleaved, const size_t p_frame_count) {
    const size_t current_write = producer_state.write_position.load(std::memory_order_relaxed);
    const size_t writable_frames =
        std::min<size_t>(get_writable_frames(current_write, p_frame_count), p_frame_count);
    if (writable_frames == 0) {
        return 0;
    }
    copy_interleaved_to_ring(p_interleaved, writable_frames, current_write);
    producer_state.write_position.store(current_write + writable_frames, std::memory_order_release);
    return writable_frames;
}

Lowl::size_l Lowl::Audio::AudioStream::write_planar(const std::vector<const Sample *> &p_channels,
                                                    const size_t p_frame_count) {
    const size_t current_write = producer_state.write_position.load(std::memory_order_relaxed);
    const size_t writable_frames =
        std::min<size_t>(get_writable_frames(current_write, p_frame_count), p_frame_count);
    if (writable_frames > 0) {
        copy_planar_to_ring(p_channels, writable_frames, current_write);
        producer_state.write_position.store(current_write + writable_frames, std::memory_order_release);
    }
    return writable_frames;
}

Lowl::size_l Lowl::Audio::AudioStream::get_frames_remaining() const {
    return get_available_frames_to_read();
}

Lowl::size_l Lowl::Audio::AudioStream::get_frame_position() const {
    return 0;
}

Lowl::size_l Lowl::Audio::AudioStream::get_frame_count() const {
    return 0;
}
