#include "lowl_audio_stream.h"

#include <algorithm>

Lowl::Audio::AudioStream::AudioStream(SampleRate p_sample_rate, AudioChannel p_channel, size_t size)
    : AudioSource(p_sample_rate, p_channel) {
    frame_capacity = size;
    const uint8_t channel_count = static_cast<uint8_t>(get_channel_num());
    channels =
        std::vector<std::vector<Sample>>(channel_count, std::vector<Sample>(frame_capacity, static_cast<Sample>(0)));
}

size_t Lowl::Audio::AudioStream::get_available_frames_to_read() const {
    const size_t current_write = write_position.load(std::memory_order_acquire);
    const size_t current_read = read_position.load(std::memory_order_relaxed);
    return current_write - current_read;
}

size_t Lowl::Audio::AudioStream::get_available_frames_to_write() const {
    return frame_capacity - get_available_frames_to_read();
}

void Lowl::Audio::AudioStream::copy_from_ring(AudioBlockView p_block,
                                              const uint32_t p_frames_to_read,
                                              const size_t p_read_position) {
    if (frame_capacity == 0 || p_frames_to_read == 0) {
        return;
    }

    const size_t first_index = p_read_position % frame_capacity;
    const uint32_t first_part_frames =
        static_cast<uint32_t>(std::min<size_t>(p_frames_to_read, frame_capacity - first_index));
    const uint32_t second_part_frames = p_frames_to_read - first_part_frames;

    for (uint8_t channel_index = 0; channel_index < p_block.channel_count; channel_index++) {
        Sample *dst = p_block.channel(channel_index);
        const std::vector<Sample> &src_channel = channels[static_cast<size_t>(channel_index)];
        std::copy_n(src_channel.data() + first_index, first_part_frames, dst);
        if (second_part_frames > 0) {
            std::copy_n(src_channel.data(), second_part_frames, dst + first_part_frames);
        }
    }
}

void Lowl::Audio::AudioStream::copy_interleaved_to_ring(const Sample *p_interleaved,
                                                        const size_t p_frame_count,
                                                        const size_t p_write_position) {
    if (frame_capacity == 0 || p_interleaved == nullptr || p_frame_count == 0) {
        return;
    }

    const size_t channel_count = channels.size();
    const size_t first_index = p_write_position % frame_capacity;
    const size_t first_part_frames = std::min(p_frame_count, frame_capacity - first_index);
    const size_t second_part_frames = p_frame_count - first_part_frames;

    for (size_t channel_index = 0; channel_index < channel_count; channel_index++) {
        std::vector<Sample> &dst_channel = channels[channel_index];
        Sample *dst = dst_channel.data() + first_index;
        const Sample *src = p_interleaved + channel_index;

        for (size_t frame_index = 0; frame_index < first_part_frames; frame_index++) {
            *dst++ = *src;
            src += channel_count;
        }

        dst = dst_channel.data();
        for (size_t frame_index = 0; frame_index < second_part_frames; frame_index++) {
            *dst++ = *src;
            src += channel_count;
        }
    }
}

void Lowl::Audio::AudioStream::copy_planar_to_ring(const std::vector<const Sample *> &p_channels,
                                                   const size_t p_frame_count,
                                                   const size_t p_write_position) {
    if (frame_capacity == 0 || p_frame_count == 0) {
        return;
    }

    const size_t channel_count = channels.size();
    const size_t first_index = p_write_position % frame_capacity;
    const size_t first_part_frames = std::min(p_frame_count, frame_capacity - first_index);
    const size_t second_part_frames = p_frame_count - first_part_frames;

    for (size_t channel_index = 0; channel_index < channel_count; channel_index++) {
        const Sample *src_channel = channel_index < p_channels.size() ? p_channels[channel_index] : nullptr;
        std::vector<Sample> &dst_channel = channels[channel_index];
        if (src_channel != nullptr) {
            std::copy_n(src_channel, first_part_frames, dst_channel.data() + first_index);
            if (second_part_frames > 0) {
                std::copy_n(src_channel + first_part_frames, second_part_frames, dst_channel.data());
            }
        } else {
            std::fill_n(dst_channel.data() + first_index, first_part_frames, static_cast<Sample>(0));
            if (second_part_frames > 0) {
                std::fill_n(dst_channel.data(), second_part_frames, static_cast<Sample>(0));
            }
        }
    }
}

Lowl::Audio::AudioSource::RenderResult Lowl::Audio::AudioStream::render(AudioBlockView p_block) {
    if (!is_playing) {
        return {0, RenderState::Starved};
    }

    const uint8_t expected_channel_count = static_cast<uint8_t>(get_channel_num());
    if (p_block.channel_count != expected_channel_count) {
        for (uint8_t channel_index = 0; channel_index < p_block.channel_count; channel_index++) {
            std::fill_n(p_block.channel(channel_index), p_block.frame_count, static_cast<Sample>(0));
        }
        return {0, RenderState::Starved};
    }

    const size_t current_read = read_position.load(std::memory_order_relaxed);
    const uint32_t frames_to_read =
        static_cast<uint32_t>(std::min<size_t>(get_available_frames_to_read(), p_block.frame_count));
    if (frames_to_read == 0) {
        return {0, RenderState::Starved};
    }

    copy_from_ring(p_block, frames_to_read, current_read);
    read_position.store(current_read + frames_to_read, std::memory_order_release);

    AudioBlockView produced_block = p_block;
    produced_block.frame_count = frames_to_read;
    process_volume(produced_block);
    process_panning(produced_block);

    return {frames_to_read, RenderState::Ok};
}

Lowl::size_l Lowl::Audio::AudioStream::write_interleaved(const Sample *p_interleaved, const size_t p_frame_count) {
    const size_t current_write = write_position.load(std::memory_order_relaxed);
    const size_t writable_frames = std::min<size_t>(get_available_frames_to_write(), p_frame_count);
    if (writable_frames == 0) {
        return 0;
    }
    copy_interleaved_to_ring(p_interleaved, writable_frames, current_write);
    write_position.store(current_write + writable_frames, std::memory_order_release);
    return writable_frames;
}

Lowl::size_l Lowl::Audio::AudioStream::write_planar(const std::vector<const Sample *> &p_channels,
                                                    const size_t p_frame_count) {
    size_t current_write = write_position.load(std::memory_order_relaxed);
    const size_t writable_frames = std::min<size_t>(get_available_frames_to_write(), p_frame_count);
    if (writable_frames > 0) {
        copy_planar_to_ring(p_channels, writable_frames, current_write);
        write_position.store(current_write + writable_frames, std::memory_order_release);
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
