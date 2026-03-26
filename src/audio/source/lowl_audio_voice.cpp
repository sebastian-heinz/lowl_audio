#include "lowl_audio_voice.h"

#include <algorithm>
#include <utility>

Lowl::Audio::AudioVoice::AudioVoice(std::shared_ptr<const AudioData> p_audio_data)
    : AudioSource(p_audio_data ? p_audio_data->get_sample_rate() : NO_SAMPLE_RATE,
                  p_audio_data ? p_audio_data->get_channel() : AudioChannel::None),
      audio_data(std::move(p_audio_data)) {
    render_position.store(0, std::memory_order_relaxed);
    reported_position.store(0, std::memory_order_release);
    playback_state.store(PlaybackState::Playing, std::memory_order_relaxed);
}

Lowl::Audio::AudioSource::RenderResult Lowl::Audio::AudioVoice::render(AudioBlockView p_block) {
    if (!audio_data) {
        playback_state.store(PlaybackState::Stopped, std::memory_order_relaxed);
        return {0, RenderState::Remove};
    }
    if (!playback_enabled.load(std::memory_order_relaxed)) {
        return {0, RenderState::Starved};
    }

    const uint8_t expected_channel_count = static_cast<uint8_t>(get_channel_num());
    if (p_block.channel_count != expected_channel_count) {
        for (uint8_t channel_index = 0; channel_index < p_block.channel_count; channel_index++) {
            std::fill_n(p_block.channel(channel_index), p_block.frame_count, static_cast<Sample>(0));
        }
        return {0, RenderState::Error};
    }

    const size_t frame_count = audio_data->get_frame_count();
    size_t current_position = render_position.load(std::memory_order_relaxed);
    const size_t pending_position = pending_seek_position.load(std::memory_order_acquire);
    if (pending_position != NoPendingSeek) {
        current_position = pending_position;
        render_position.store(current_position, std::memory_order_relaxed);
        reported_position.store(current_position, std::memory_order_release);

        size_t expected_pending_position = pending_position;
        pending_seek_position.compare_exchange_strong(expected_pending_position,
                                                     NoPendingSeek,
                                                     std::memory_order_acq_rel,
                                                     std::memory_order_acquire);
    }
    if (current_position >= frame_count || p_block.frame_count == 0) {
        render_position.store(0, std::memory_order_relaxed);
        reported_position.store(0, std::memory_order_release);
        playback_state.store(PlaybackState::Stopped, std::memory_order_relaxed);
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
        render_position.store(0, std::memory_order_relaxed);
        reported_position.store(0, std::memory_order_release);
        playback_state.store(PlaybackState::Stopped, std::memory_order_relaxed);
        return {frames_to_copy, RenderState::Remove};
    }
    render_position.store(current_position, std::memory_order_relaxed);
    reported_position.store(current_position, std::memory_order_release);
    return {frames_to_copy, RenderState::Ok};
}

Lowl::size_l Lowl::Audio::AudioVoice::get_frames_remaining() const {
    if (!audio_data) {
        return 0;
    }
    const size_t current_position = get_frame_position();
    const size_t frame_count = audio_data->get_frame_count();
    if (current_position >= frame_count) {
        return 0;
    }
    return frame_count - current_position;
}

Lowl::size_l Lowl::Audio::AudioVoice::get_frame_position() const {
    const size_t pending_position = pending_seek_position.load(std::memory_order_acquire);
    if (pending_position != NoPendingSeek) {
        return pending_position;
    }
    return reported_position.load(std::memory_order_acquire);
}

Lowl::size_l Lowl::Audio::AudioVoice::get_frame_count() const {
    if (!audio_data) {
        return 0;
    }
    return audio_data->get_frame_count();
}

void Lowl::Audio::AudioVoice::reset() {
    pending_seek_position.store(0, std::memory_order_release);
    reported_position.store(0, std::memory_order_release);
    detached.store(false, std::memory_order_relaxed);
}

void Lowl::Audio::AudioVoice::seek_time(const TimeSeconds p_seconds) {
    const size_t frame = static_cast<size_t>(p_seconds * sample_rate);
    seek_frame(frame);
}

void Lowl::Audio::AudioVoice::seek_frame(size_t p_frame) {
    size_t target_position = 0;
    if (!audio_data || audio_data->get_frame_count() == 0) {
        pending_seek_position.store(0, std::memory_order_release);
        reported_position.store(0, std::memory_order_release);
        return;
    }
    target_position = std::min<size_t>(p_frame, audio_data->get_frame_count() - 1);
    pending_seek_position.store(target_position, std::memory_order_release);
    reported_position.store(target_position, std::memory_order_release);
}

bool Lowl::Audio::AudioVoice::is_detached() const {
    return detached.load(std::memory_order_relaxed);
}

Lowl::Audio::AudioVoice::PlaybackState Lowl::Audio::AudioVoice::get_playback_state() const {
    return playback_state.load(std::memory_order_relaxed);
}

void Lowl::Audio::AudioVoice::restart_playback() {
    reset();
    play();
    playback_state.store(PlaybackState::Playing, std::memory_order_relaxed);
}

void Lowl::Audio::AudioVoice::pause_playback() {
    pause();
    if (playback_state.load(std::memory_order_relaxed) != PlaybackState::Stopped) {
        playback_state.store(PlaybackState::Paused, std::memory_order_relaxed);
    }
}

void Lowl::Audio::AudioVoice::resume_playback() {
    play();
    playback_state.store(PlaybackState::Playing, std::memory_order_relaxed);
}

void Lowl::Audio::AudioVoice::stop_playback() {
    pause();
    playback_state.store(PlaybackState::Stopped, std::memory_order_relaxed);
    reset();
}

void Lowl::Audio::AudioVoice::on_added_to_mixer() {
    detached.store(false, std::memory_order_relaxed);
}

void Lowl::Audio::AudioVoice::on_removed_from_mixer() {
    detached.store(true, std::memory_order_relaxed);
}
