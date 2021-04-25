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
        std::atomic<size_l> frames_remaining;

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

        AudioMixer(SampleRate p_sample_rate, Channel p_channel, Volume p_volume = 1.0, Panning p_panning = 0.5);

        virtual ~AudioMixer() = default;

#ifdef LOWL_PROFILING
        public:
            uint64_t mix_frame_count;
            double mix_total_duration;
            double mix_max_duration;
            double mix_min_duration;
            double mix_avg_duration;
#endif
    };
}


#endif //LOWL_AUDIO_MIXER_H
