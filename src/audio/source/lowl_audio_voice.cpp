#include "lowl_audio_voice.h"

#include <algorithm>

Lowl::Audio::AudioVoice::AudioVoice(const AudioData *p_audio_data)
    : AudioSource(
        p_audio_data ? p_audio_data->get_sample_rate() : NO_SAMPLE_RATE,
        p_audio_data ? p_audio_data->get_channel() : AudioChannel::None
    ) {
    audio_data = p_audio_data;
    position = 0;
    seek_position = 0;
    is_not_reset.test_and_set();
}

Lowl::Audio::AudioSource::RenderResult Lowl::Audio::AudioVoice::render(AudioBlockView p_block) {
    if (!audio_data) {
        return {0, RenderState::Remove};
    }
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

    if (!is_not_reset.test_and_set()) {
        position = seek_position.load();
        seek_position = 0;
    }

    const size_t frame_count = audio_data->get_frame_count();
    size_t current_position = position.load(std::memory_order_relaxed);
    if (current_position >= frame_count || p_block.frame_count == 0) {
        position.store(0, std::memory_order_relaxed);
        return {0, RenderState::Remove};
    }

    const size_t available = frame_count - current_position;
    const uint32_t frames_to_copy = static_cast<uint32_t>(std::min<size_t>(available, p_block.frame_count));
    const uint8_t channel_count = expected_channel_count;

    for (uint8_t channel_index = 0; channel_index < channel_count; channel_index++) {
        const Sample *src_channel = audio_data->get_channel_data(channel_index);
        if (src_channel != nullptr) {
            std::copy_n(src_channel + current_position, frames_to_copy, p_block.channel(channel_index));
        }
    }

    AudioBlockView produced_block = p_block;
    produced_block.frame_count = frames_to_copy;
    process_volume(produced_block);
    process_panning(produced_block);

    current_position += frames_to_copy;
    if (current_position >= frame_count) {
        position.store(0, std::memory_order_relaxed);
        return {frames_to_copy, RenderState::Remove};
    }
    position.store(current_position, std::memory_order_relaxed);
    return {frames_to_copy, RenderState::Ok};
}

Lowl::size_l Lowl::Audio::AudioVoice::get_frames_remaining() const {
    if (!audio_data) {
        return 0;
    }
    const int64_t remaining = static_cast<int64_t>(audio_data->get_frame_count() - position.load());
    return static_cast<size_l>(std::max<int64_t>(remaining, 0));
}

Lowl::size_l Lowl::Audio::AudioVoice::get_frame_position() const {
    return position.load();
}

Lowl::size_l Lowl::Audio::AudioVoice::get_frame_count() const {
    if (!audio_data) {
        return 0;
    }
    return audio_data->get_frame_count();
}

void Lowl::Audio::AudioVoice::reset() {
    seek_position = 0;
    is_not_reset.clear();
    detached.store(false, std::memory_order_relaxed);
}

void Lowl::Audio::AudioVoice::seek_time(const TimeSeconds p_seconds) {
    const size_t frame = static_cast<size_t>(p_seconds * sample_rate);
    seek_frame(frame);
}

void Lowl::Audio::AudioVoice::seek_frame(size_t p_frame) {
    if (!audio_data || audio_data->get_frame_count() == 0) {
        seek_position = 0;
        is_not_reset.clear();
        return;
    }
    p_frame = std::min<size_t>(p_frame, audio_data->get_frame_count() - 1);
    seek_position = p_frame;
    is_not_reset.clear();
}

bool Lowl::Audio::AudioVoice::is_detached() const {
    return detached.load(std::memory_order_relaxed);
}

void Lowl::Audio::AudioVoice::on_removed_from_mixer() {
    detached.store(true, std::memory_order_relaxed);
}
