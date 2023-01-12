#ifndef LOWL_RE_SAMPLER_H
#define LOWL_RE_SAMPLER_H

#include "lowl_typedef.h"
#include "lowl_error.h"

#include "audio/lowl_audio_sample_format.h"
#include "audio/lowl_audio_frame.h"
#include "audio/lowl_audio_channel.h"


namespace Lowl::Audio {
    class ReSampler {

    protected:
        SampleRate sample_rate_src;
        SampleRate sample_rate_dst;
        AudioChannel channel;

    public:
        ReSampler(SampleRate p_sample_rate_src,
                  SampleRate p_sample_rate_dst,
                  AudioChannel p_channel
        );

        virtual bool read(AudioFrame &audio_frame) = 0;

        virtual void write(const AudioFrame &p_audio_frame) = 0;

        virtual void flush();

        virtual ~ReSampler() = default;
    };
}

#endif
