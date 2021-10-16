#ifndef LOWL_AUDIO_MIXER_H
#define LOWL_AUDIO_MIXER_H

#include "lowl_typedef.h"
#include "lowl_audio_mixer_event.h"
#include "lowl_audio_source.h"

#include <concurrentqueue.h>

#include <vector>

namespace Lowl {

    class AudioMixer : public AudioSource {

    private:
        std::vector<std::shared_ptr<AudioSource>> sources;
        std::unique_ptr<moodycamel::ConcurrentQueue<AudioMixerEvent>> events;
        AudioFrame read_frame;

    public:
        virtual size_l get_frames_remaining() const override;

        virtual size_l get_frame_position() const override;

        virtual size_l get_frame_count() const override;

        /**
         * mixes a single frame from all sources
         */
        virtual ReadResult read(AudioFrame &audio_frame) override;

        /**
         * adds a audio source to mix
         */
        virtual void mix(std::shared_ptr<AudioSource> p_audio_source);

        /**
         * removes a audio source from the mix
         */
        virtual void remove(std::shared_ptr<AudioSource> p_audio_source);

        AudioMixer(SampleRate p_sample_rate, AudioChannel p_channel);

        virtual ~AudioMixer() = default;
    };
}


#endif //LOWL_AUDIO_MIXER_H
