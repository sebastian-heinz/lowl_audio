#ifndef LOWL_AUDIO_VOICE_H
#define LOWL_AUDIO_VOICE_H

#include "audio/source/lowl_audio_data.h"
#include "audio/source/lowl_audio_source.h"

#include <atomic>

namespace Lowl::Audio {
    class AudioVoice : public AudioSource {
    private:
        const AudioData *audio_data = nullptr;
        std::atomic<size_t> position{};
        std::atomic<size_t> seek_position{};
        std::atomic_flag is_not_reset{};
        std::atomic<bool> detached{false};

    public:
        explicit AudioVoice(const AudioData *p_audio_data);

        RenderResult render(AudioBlockView p_block) override;
        size_l get_frames_remaining() const override;
        size_l get_frame_position() const override;
        size_l get_frame_count() const override;

        void reset();
        void seek_time(TimeSeconds p_seconds);
        void seek_frame(size_t p_frame);
        bool is_detached() const;

        void on_removed_from_mixer() override;
    };
}

#endif
