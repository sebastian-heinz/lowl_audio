#include "lowl_audio_voice.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

void Lowl::Audio::AudioVoice::begin_control_state_transition() {
    control_state_serial.fetch_add(1, std::memory_order_acq_rel);
}

void Lowl::Audio::AudioVoice::end_control_state_transition() {
    control_state_serial.fetch_add(1, std::memory_order_acq_rel);
}

Lowl::Audio::AudioVoice::AudioVoice(std::shared_ptr<const AudioData> p_audio_data)
    : AudioSource(p_audio_data ? p_audio_data->get_audio_format() : AudioFormat{}),
      audio_data(std::move(p_audio_data)) {
    render_position.store(0, std::memory_order_relaxed);
    pause();
    published_state.store(PlaybackSnapshot{0, PlaybackState::Stopped});
}

Lowl::Audio::AudioSource::RenderResult Lowl::Audio::AudioVoice::mix_into(AudioBlockView p_block,
                                                                         const MixGainVector &p_upstream_gain) {
    if (!audio_data) {
        published_state.try_update([](PlaybackSnapshot &p_snapshot) {
            p_snapshot.frame_position = 0;
            p_snapshot.playback_state = PlaybackState::Stopped;
        });
        return {0, RenderState::Remove};
    }
    if (!playback_enabled.load(std::memory_order_acquire)) {
        return {0, RenderState::Starved};
    }

    const uint8_t expected_channel_count = get_channel_count();
    if (p_block.channel_count != expected_channel_count) {
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
            published_state.try_update([](PlaybackSnapshot &p_snapshot) {
                p_snapshot.frame_position = 0;
                p_snapshot.playback_state = PlaybackState::Stopped;
            });
        }
        return {0, RenderState::Remove};
    }

    const size_t available = frame_count - current_position;
    const uint32_t frames_to_copy = static_cast<uint32_t>(std::min<size_t>(available, p_block.frame_count));
    const uint8_t channel_count = expected_channel_count;
    const MixGainVector effective_gain = compose_gain_vector(p_upstream_gain);

    for (uint8_t channel_index = 0; channel_index < channel_count; channel_index++) {
        const Sample gain = effective_gain[channel_index];
        if (std::abs(gain) <= std::numeric_limits<Sample>::epsilon()) {
            continue;
        }

        const Sample *src_channel = audio_data->get_channel_data(channel_index);
        if (src_channel != nullptr) {
            mix_scaled_channel(src_channel + current_position, p_block.channel(channel_index), gain, frames_to_copy);
        }
    }

    current_position += frames_to_copy;
    if (current_position >= frame_count) {
        render_position.store(0, std::memory_order_relaxed);
        if ((observed_control_state_serial & 0x1U) == 0U &&
            control_state_serial.load(std::memory_order_acquire) == observed_control_state_serial) {
            published_state.try_update([](PlaybackSnapshot &p_snapshot) {
                p_snapshot.frame_position = 0;
                p_snapshot.playback_state = PlaybackState::Stopped;
            });
        }
        return {frames_to_copy, RenderState::Remove};
    }
    render_position.store(current_position, std::memory_order_relaxed);
    if ((observed_control_state_serial & 0x1U) == 0U &&
        control_state_serial.load(std::memory_order_acquire) == observed_control_state_serial) {
        published_state.try_update([current_position](PlaybackSnapshot &p_snapshot) {
            p_snapshot.frame_position = current_position;
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
    return get_playback_snapshot().frame_position;
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
    published_state.update([](PlaybackSnapshot &p_snapshot) {
        p_snapshot.frame_position = 0;
    });
    end_control_state_transition();
}

void Lowl::Audio::AudioVoice::seek_time(const TimeSeconds p_seconds) {
    const TimeSeconds clamped_seconds = std::max<TimeSeconds>(0.0, p_seconds);
    const size_t frame = static_cast<size_t>(clamped_seconds * get_audio_format().sample_rate);
    seek_frame(frame);
}

void Lowl::Audio::AudioVoice::seek_frame(size_t p_frame) {
    std::lock_guard<std::mutex> lock(control_state_mutex);
    size_t target_position = 0;
    begin_control_state_transition();
    if (!audio_data || audio_data->get_frame_count() == 0) {
        pending_seek_position.store(0, std::memory_order_release);
        published_state.update([](PlaybackSnapshot &p_snapshot) {
            p_snapshot.frame_position = 0;
        });
        end_control_state_transition();
        return;
    }
    target_position = std::min<size_t>(p_frame, audio_data->get_frame_count() - 1);
    pending_seek_position.store(target_position, std::memory_order_release);
    published_state.update([target_position](PlaybackSnapshot &p_snapshot) {
        p_snapshot.frame_position = target_position;
    });
    end_control_state_transition();
}

Lowl::Audio::AudioVoice::PlaybackSnapshot Lowl::Audio::AudioVoice::get_playback_snapshot() const {
    return published_state.load();
}

Lowl::Audio::AudioVoice::PlaybackState Lowl::Audio::AudioVoice::get_playback_state() const {
    return get_playback_snapshot().playback_state;
}

void Lowl::Audio::AudioVoice::restart_playback() {
    std::lock_guard<std::mutex> lock(control_state_mutex);
    begin_control_state_transition();
    pending_seek_position.store(0, std::memory_order_release);
    published_state.update([](PlaybackSnapshot &p_snapshot) {
        p_snapshot.frame_position = 0;
        p_snapshot.playback_state = PlaybackState::Playing;
    });
    play();
    end_control_state_transition();
}

void Lowl::Audio::AudioVoice::pause_playback() {
    std::lock_guard<std::mutex> lock(control_state_mutex);
    begin_control_state_transition();
    pause();
    published_state.update([](PlaybackSnapshot &p_snapshot) {
        if (p_snapshot.playback_state != PlaybackState::Stopped) {
            p_snapshot.playback_state = PlaybackState::Paused;
        }
    });
    end_control_state_transition();
}

void Lowl::Audio::AudioVoice::resume_playback() {
    std::lock_guard<std::mutex> lock(control_state_mutex);
    begin_control_state_transition();
    published_state.update([](PlaybackSnapshot &p_snapshot) {
        p_snapshot.playback_state = PlaybackState::Playing;
    });
    play();
    end_control_state_transition();
}

void Lowl::Audio::AudioVoice::stop_playback() {
    std::lock_guard<std::mutex> lock(control_state_mutex);
    begin_control_state_transition();
    pause();
    pending_seek_position.store(0, std::memory_order_release);
    published_state.update([](PlaybackSnapshot &p_snapshot) {
        p_snapshot.frame_position = 0;
        p_snapshot.playback_state = PlaybackState::Stopped;
    });
    end_control_state_transition();
}
