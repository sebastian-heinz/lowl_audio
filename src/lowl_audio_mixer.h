#ifndef LOWL_AUDIO_MIXER_H
#define LOWL_AUDIO_MIXER_H

#include "lowl_audio_stream.h"

#include <readerwriterqueue.h>

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
        std::shared_ptr<AudioStream> out_stream;

    protected:
        virtual void mix_thread();

        virtual void mix_frame();

    public:
        virtual ~AudioMixer();

        virtual void start_mix();

        virtual void stop_mix();

        virtual void mix_all();

        virtual void mix_stream(std::shared_ptr<AudioStream> p_audio_stream);

        SampleRate get_sample_rate() const;

        Channel get_channel() const;

        std::shared_ptr<AudioStream> get_out_stream();

        AudioMixer(SampleRate p_sample_rate, Channel p_channel);

    };
}


#endif //LOWL_AUDIO_MIXER_H
