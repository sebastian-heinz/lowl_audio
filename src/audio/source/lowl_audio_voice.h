#ifndef LOWL_AUDIO_VOICE_H
#define LOWL_AUDIO_VOICE_H

#include <atomic>
#include <limits>

#include "audio/source/lowl_audio_data.h"
#include "audio/source/lowl_audio_source.h"

namespace Lowl::Audio {
    /**
     * Playback instance for an `AudioData` clip with its own position, gain, and panning state.
     */
    class AudioVoice : public AudioSource {
    public:
        enum class PlaybackState : uint8_t {
            Stopped = 0,
            Playing = 1,
            Paused = 2,
        };

    private:
        static constexpr size_t NoPendingSeek = std::numeric_limits<size_t>::max();

        std::shared_ptr<const AudioData> audio_data;
        std::atomic<size_t> render_position{};
        std::atomic<size_t> reported_position{};
        std::atomic<size_t> pending_seek_position{NoPendingSeek};
        std::atomic<bool> detached{false};
        std::atomic<PlaybackState> playback_state{PlaybackState::Stopped};

    public:
        explicit AudioVoice(std::shared_ptr<const AudioData> p_audio_data);

        RenderResult render(AudioBlockView p_block) override;
        size_l get_frames_remaining() const override;
        size_l get_frame_position() const override;
        size_l get_frame_count() const override;

        void reset();
        void seek_time(TimeSeconds p_seconds);
        void seek_frame(size_t p_frame);
        bool is_detached() const;
        PlaybackState get_playback_state() const;
        void restart_playback();
        void pause_playback();
        void resume_playback();
        void stop_playback();

        void on_added_to_mixer() override;
        void on_removed_from_mixer() override;
    };
} // namespace Lowl::Audio

#endif
