#ifndef LOWL_AUDIO_MIXER_H
#define LOWL_AUDIO_MIXER_H

#include "lowl_typedef.h"

#include "audio/source/lowl_audio_mixer_event.h"
#include "audio/source/lowl_audio_source.h"

#include <concurrentqueue.h>

#include <vector>

namespace Lowl::Audio {
    class AudioMixer : public AudioSource {
    private:
        std::vector<std::shared_ptr<AudioSource> > sources;
        std::unique_ptr<moodycamel::ConcurrentQueue<AudioMixerEvent> > events;
        AudioFrame read_frame;
        std::atomic<bool> normalize_output{true};

    public:
        size_l get_frames_remaining() const override;

        size_l get_frame_position() const override;

        size_l get_frame_count() const override;

        /**
         * mixes a single frame from all sources
         */
        ReadResult read(AudioFrame &audio_frame) override;

        /**
         * adds a audio source to mix
         */
        virtual void mix(std::shared_ptr<AudioSource> p_audio_source);

        /**
         * removes a audio source from the mix
         */
        virtual void remove(std::shared_ptr<AudioSource> p_audio_source);

        void set_normalize_output(bool p_normalize);

        AudioMixer(SampleRate p_sample_rate, AudioChannel p_channel);

        ~AudioMixer() override = default;
    };
}


#endif //LOWL_AUDIO_MIXER_H
