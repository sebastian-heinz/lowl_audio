#ifndef LOWL_RE_SAMPLER_H
#define LOWL_RE_SAMPLER_H

#include "lowl_sample_format.h"
#include "lowl_error.h"
#include "lowl_audio_frame.h"
#include "lowl_channel.h"
#include "lowl_sample_rate.h"

#include <readerwriterqueue.h>
#include <CDSPResampler.h>

#include <vector>

namespace Lowl {
    class ReSampler {
    private:
        SampleRate sample_rate_src;
        SampleRate sample_rate_dst;
        Channel channel;
        int num_channel;
        size_t current_frame;
        size_t sample_available;
        std::vector<AudioFrame> resamples;
        std::vector<std::vector<double>> samples;
        std::vector<std::unique_ptr<r8b::CDSPResampler24>> re_samplers;
        size_t sample_buffer_size;
        moodycamel::ReaderWriterQueue<AudioFrame> *resample_queue;


    public:
        ReSampler(SampleRate p_sample_rate_src,
                  SampleRate p_sample_rate_dst,
                  Channel p_channel,
                  size_t p_sample_buffer_size
        );


        bool read(AudioFrame &audio_frame);

        /**
         * Returns true if number of required_frames is available
         */
        bool write(const AudioFrame &p_audio_frame, size_t p_required_frames);

        virtual ~ReSampler() = default;
    };
}

#endif