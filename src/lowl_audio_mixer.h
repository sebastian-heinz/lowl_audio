#ifndef LOWL_AUDIO_MIXER_H
#define LOWL_AUDIO_MIXER_H

#include "lowl_typedef.h"
#include "lowl_audio_stream.h"
#include "lowl_audio_data.h"
#include "lowl_audio_mixer_event.h"
#include "lowl_audio_source.h"

#include <concurrentqueue.h>

#include <vector>

namespace Lowl {

    class AudioMixer : public AudioSource {

    private:
        std::vector<std::shared_ptr<AudioStream>> streams;
        std::vector<std::shared_ptr<AudioData>> data;
        std::vector<std::shared_ptr<AudioMixer>> mixers;
        std::unique_ptr<moodycamel::ConcurrentQueue<AudioMixerEvent>> events;
        AudioFrame read_frame;

    public:
        virtual size_l get_frames_remaining() const override;

        /**
         * mixes a single frame from all sources
         */
        virtual bool read(AudioFrame &audio_frame) override;

        /**
         * adds a stream to mix
         */
        virtual void mix_stream(std::shared_ptr<AudioStream> p_audio_stream);

        /**
         * adds a data to mix
         */
        virtual void mix_data(std::shared_ptr<AudioData> p_audio_data);

        /**
         * adds a audio source to mix
         */
        virtual void mix(std::shared_ptr<AudioSource> p_audio_source);

        /**
         * adds a mixer to mix
         */
        virtual void mix_mixer(std::shared_ptr<AudioMixer> p_audio_mixer);

        /**
         * removes a stream from the mix
         */
        virtual void remove_stream(std::shared_ptr<AudioStream> p_audio_stream);

        /**
         * removes data from the mix
         */
        virtual void remove_data(std::shared_ptr<AudioData> p_audio_data);

        /**
         * removes a audio source from the mix
         */
        virtual void remove(std::shared_ptr<AudioSource> p_audio_source);

        /**
         * removes a mixer from the mix
         */
        virtual void remove_mixer(std::shared_ptr<AudioMixer> p_audio_mixer);

        AudioMixer(SampleRate p_sample_rate, Channel p_channel, Volume p_volume = 1.0, Panning p_panning = 0.5);

        virtual ~AudioMixer() = default;
    };
}


#endif //LOWL_AUDIO_MIXER_H
