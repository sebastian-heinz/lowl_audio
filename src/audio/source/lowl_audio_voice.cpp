#include "lowl_audio_voice.h"

#include <algorithm>
#include <utility>

void Lowl::Audio::AudioVoice::begin_control_state_transition() {
    control_state_serial.fetch_add(1, std::memory_order_acq_rel);
}

void Lowl::Audio::AudioVoice::end_control_state_transition() {
    control_state_serial.fetch_add(1, std::memory_order_acq_rel);
}

Lowl::Audio::AudioVoice::AudioVoice(std::shared_ptr<const AudioData> p_audio_data)
    : AudioSource(p_audio_data ? p_audio_data->get_sample_rate() : NO_SAMPLE_RATE,
                  p_audio_data ? p_audio_data->get_channel_layout() : ChannelLayout{}),
      audio_data(std::move(p_audio_data)) {
    render_position.store(0, std::memory_order_relaxed);
    pause();
    published_state.store(PublishedStateSnapshot{0, PlaybackState::Stopped, false});
}

Lowl::Audio::AudioSource::RenderResult Lowl::Audio::AudioVoice::render(AudioBlockView p_block) {
    if (!audio_data) {
        published_state.update([](PublishedStateSnapshot &p_state) {
            p_state.position = 0;
            p_state.playback_state = PlaybackState::Stopped;
        });
        return {0, RenderState::Remove};
    }
    if (!playback_enabled.load(std::memory_order_acquire)) {
        return {0, RenderState::Starved};
    }

    const uint8_t expected_channel_count = get_channel_count();
    if (p_block.channel_count != expected_channel_count) {
        for (uint8_t channel_index = 0; channel_index < p_block.channel_count; channel_index++) {
            std::fill_n(p_block.channel(channel_index), p_block.frame_count, static_cast<Sample>(0));
        }
        return {0, RenderState::Error};
    }

    const size_t frame_count = audio_data->get_frame_count();
    size_t current_position = render_position.load(std::memory_order_relaxed);
    const uint64_t observed_control_state_serial = control_state_serial.load(std::memory_order_acquire);
    const size_t pending_position = pending_seek_position.load(std::memory_order_acquire);
    if (pending_position != NoPendingSeek) {
        current_position = pending_position;
        render_position.store(current_position, std::memory_order_relaxed);

        size_t expected_pending_position = pending_position;
        pending_seek_position.compare_exchange_strong(expected_pending_position,
                                                     NoPendingSeek,
                                                     std::memory_order_acq_rel,
                                                     std::memory_order_acquire);
    }
    if (p_block.frame_count == 0) {
        return {0, RenderState::Ok};
    }
    if (current_position >= frame_count) {
        render_position.store(0, std::memory_order_relaxed);
        if ((observed_control_state_serial & 0x1U) == 0U &&
            control_state_serial.load(std::memory_order_acquire) == observed_control_state_serial) {
            published_state.update([](PublishedStateSnapshot &p_state) {
                p_state.position = 0;
                p_state.playback_state = PlaybackState::Stopped;
            });
        }
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
        if ((observed_control_state_serial & 0x1U) == 0U &&
            control_state_serial.load(std::memory_order_acquire) == observed_control_state_serial) {
            published_state.update([](PublishedStateSnapshot &p_state) {
                p_state.position = 0;
                p_state.playback_state = PlaybackState::Stopped;
            });
        }
        return {frames_to_copy, RenderState::Remove};
    }
    render_position.store(current_position, std::memory_order_relaxed);
    if ((observed_control_state_serial & 0x1U) == 0U &&
        control_state_serial.load(std::memory_order_acquire) == observed_control_state_serial) {
        published_state.update([current_position](PublishedStateSnapshot &p_state) {
            p_state.position = static_cast<uint32_t>(current_position);
        });
    }
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
    return published_state.load().position;
}

Lowl::size_l Lowl::Audio::AudioVoice::get_frame_count() const {
    if (!audio_data) {
        return 0;
    }
    return audio_data->get_frame_count();
}

void Lowl::Audio::AudioVoice::reset() {
    std::lock_guard<std::mutex> lock(control_state_mutex);
    begin_control_state_transition();
    pending_seek_position.store(0, std::memory_order_release);
    published_state.update([](PublishedStateSnapshot &p_state) {
        p_state.position = 0;
    });
    end_control_state_transition();
}

void Lowl::Audio::AudioVoice::seek_time(const TimeSeconds p_seconds) {
    const TimeSeconds clamped_seconds = std::max<TimeSeconds>(0.0, p_seconds);
    const size_t frame = static_cast<size_t>(clamped_seconds * sample_rate);
    seek_frame(frame);
}

void Lowl::Audio::AudioVoice::seek_frame(size_t p_frame) {
    std::lock_guard<std::mutex> lock(control_state_mutex);
    size_t target_position = 0;
    begin_control_state_transition();
    if (!audio_data || audio_data->get_frame_count() == 0) {
        pending_seek_position.store(0, std::memory_order_release);
        published_state.update([](PublishedStateSnapshot &p_state) {
            p_state.position = 0;
        });
        end_control_state_transition();
        return;
    }
    target_position = std::min<size_t>(p_frame, audio_data->get_frame_count() - 1);
    pending_seek_position.store(target_position, std::memory_order_release);
    published_state.update([target_position](PublishedStateSnapshot &p_state) {
        p_state.position = static_cast<uint32_t>(target_position);
    });
    end_control_state_transition();
}

bool Lowl::Audio::AudioVoice::is_detached() const {
    return published_state.load().detached;
}

Lowl::Audio::AudioVoice::PlaybackState Lowl::Audio::AudioVoice::get_playback_state() const {
    return published_state.load().playback_state;
}

void Lowl::Audio::AudioVoice::restart_playback() {
    std::lock_guard<std::mutex> lock(control_state_mutex);
    begin_control_state_transition();
    pending_seek_position.store(0, std::memory_order_release);
    published_state.update([](PublishedStateSnapshot &p_state) {
        p_state.position = 0;
        p_state.playback_state = PlaybackState::Playing;
    });
    play();
    end_control_state_transition();
}

void Lowl::Audio::AudioVoice::pause_playback() {
    std::lock_guard<std::mutex> lock(control_state_mutex);
    begin_control_state_transition();
    pause();
    published_state.update([](PublishedStateSnapshot &p_state) {
        if (p_state.playback_state != PlaybackState::Stopped) {
            p_state.playback_state = PlaybackState::Paused;
        }
    });
    end_control_state_transition();
}

void Lowl::Audio::AudioVoice::resume_playback() {
    std::lock_guard<std::mutex> lock(control_state_mutex);
    begin_control_state_transition();
    published_state.update([](PublishedStateSnapshot &p_state) {
        p_state.playback_state = PlaybackState::Playing;
    });
    play();
    end_control_state_transition();
}

void Lowl::Audio::AudioVoice::stop_playback() {
    std::lock_guard<std::mutex> lock(control_state_mutex);
    begin_control_state_transition();
    pause();
    pending_seek_position.store(0, std::memory_order_release);
    published_state.update([](PublishedStateSnapshot &p_state) {
        p_state.position = 0;
        p_state.playback_state = PlaybackState::Stopped;
    });
    end_control_state_transition();
}

void Lowl::Audio::AudioVoice::on_added_to_mixer() {
    published_state.update([](PublishedStateSnapshot &p_state) {
        p_state.detached = false;
    });
}

void Lowl::Audio::AudioVoice::on_removed_from_mixer() {
    published_state.update([](PublishedStateSnapshot &p_state) {
        p_state.detached = true;
    });
}
