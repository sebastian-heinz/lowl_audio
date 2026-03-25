#ifndef LOWL_AUDIO_MIXER_H
#define LOWL_AUDIO_MIXER_H

#include "lowl_typedef.h"

#include "audio/source/lowl_audio_mixer_event.h"
#include "audio/source/lowl_audio_source.h"

#include <concurrentqueue.h>

#include <array>

namespace Lowl::Audio {
    class AudioMixer : public AudioSource {
    private:
        static constexpr uint32_t SCRATCH_BUFFER_CAPACITY = 8192;
        static constexpr size_t MAX_ACTIVE_SOURCES = 1024;

        std::array<AudioSource *, MAX_ACTIVE_SOURCES> sources{};
        std::unique_ptr<moodycamel::ConcurrentQueue<AudioMixerEvent> > events;
        AudioBuffer scratch_buffer;

        size_t find_source_index(const AudioSource *p_audio_source) const;
        size_t find_free_source_index() const;

    public:
        size_l get_frames_remaining() const override;

        size_l get_frame_position() const override;

        size_l get_frame_count() const override;

        /**
         * mixes a block from all sources
         */
        RenderResult render(AudioBlockView p_block) override;

        /**
         * adds a audio source to mix
         */
        virtual void mix(AudioSource *p_audio_source);

        /**
         * removes a audio source from the mix
         */
        virtual void remove(AudioSource *p_audio_source);

        AudioMixer(SampleRate p_sample_rate, AudioChannel p_channel);

        ~AudioMixer() override = default;
    };
}


#endif //LOWL_AUDIO_MIXER_H
