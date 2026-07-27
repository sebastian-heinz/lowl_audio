#ifndef LOWL_AUDIO_VOICE_H
#define LOWL_AUDIO_VOICE_H

#include <atomic>
#include <cstdint>
#include <limits>
#include <mutex>

#include "audio/source/lowl_audio_data.h"
#include "audio/source/lowl_audio_published_playback_state.h"
#include "audio/source/lowl_audio_source.h"

namespace Lowl::Audio {
    /**
     * Playback instance for an `AudioData` asset with its own position, gain, and panning state.
     */
    class AudioVoice : public AudioSource {
        static_assert(std::atomic<size_t>::is_always_lock_free,
                      "AudioVoice positions must be lock-free for real-time audio safety");

    public:
        enum class PlaybackState : uint8_t {
            Stopped = 0,
            Playing = 1,
            Paused = 2,
        };

        /** Coherent intrinsic voice state; mixer attachment belongs to the owning composition layer. */
        struct PlaybackSnapshot {
            size_l frame_position = 0;
            PlaybackState playback_state = PlaybackState::Stopped;
        };

    private:
        static constexpr size_t NoPendingSeek = std::numeric_limits<size_t>::max();

        using PublishedState = Detail::PublishedPlaybackState<PlaybackSnapshot, PlaybackState, Sample>;

        std::shared_ptr<const AudioData> audio_data;
        mutable std::mutex control_state_mutex;
        std::atomic<size_t> render_position{};
        std::atomic<size_t> pending_seek_position{NoPendingSeek};
        PublishedState published_state{};
        std::atomic<uint64_t> control_state_serial{0};

        void begin_control_state_transition();
        void end_control_state_transition();

    public:
        explicit AudioVoice(std::shared_ptr<const AudioData> p_audio_data);

        RenderResult mix_into(AudioBlockView p_block, const MixGainVector &p_upstream_gain) override;
        size_l get_frames_remaining() const override;
        size_l get_frame_position() const override;
        size_l get_frame_count() const override;

        void reset();
        void seek_time(TimeSeconds p_seconds);
        void seek_frame(size_t p_frame);
        /** Use this snapshot when a decision depends on more than one published playback field. */
        PlaybackSnapshot get_playback_snapshot() const;
        PlaybackState get_playback_state() const;
        void restart_playback();
        void pause_playback();
        void resume_playback();
        void stop_playback();
    };
} // namespace Lowl::Audio

#endif
