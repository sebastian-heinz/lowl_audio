#ifndef LOWL_AUDIO_MIXER_H
#define LOWL_AUDIO_MIXER_H

#include "lowl_audio_stream.h"
#include "lowl_audio_data.h"
#include "lowl_audio_mixer_event.h"

#include <concurrentqueue.h>

#include <thread>
#include <vector>

namespace Lowl {
    class AudioMixer {


    private:
        bool running;
        SampleRate sample_rate;
        Channel channel;
        std::thread thread;
        std::vector<std::shared_ptr<AudioStream>> streams;
        std::vector<std::shared_ptr<AudioData>> data;
        std::shared_ptr<AudioStream> out_stream;
        std::unique_ptr<moodycamel::ConcurrentQueue<AudioMixerEvent>> events;

    protected:
        virtual void mix_thread();

    public:
        virtual ~AudioMixer();

        /**
         * mixes a single frame from all sources
         */
        virtual bool mix_next_frame();

        /**
         * starts to continuously mixing all inputs
         */
        virtual void start_mix();

        /**
         * stops all mixing operations instantly and clears all data.
         */
        virtual void stop_mix();

        /**
         * mixes until all inputs are exhausted
         */
        virtual void mix_all();

        /**
         * adds a stream to mix
         */
        virtual void mix_stream(std::shared_ptr<AudioStream> p_audio_stream);

        /**
         * adds a frame to mix
         */
        virtual void mix_data(std::shared_ptr<AudioData> p_audio_data);

        SampleRate get_sample_rate() const;

        Channel get_channel() const;

        std::shared_ptr<AudioStream> get_out_stream();

        AudioMixer(SampleRate p_sample_rate, Channel p_channel);

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
