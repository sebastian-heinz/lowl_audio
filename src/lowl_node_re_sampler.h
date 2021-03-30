#ifndef LOWL_NODE_RE_SAMPLER_H
#define LOWL_NODE_RE_SAMPLER_H

#include "lowl_audio_frame.h"
#include "lowl_re_sampler.h"

#include <memory>

namespace Lowl {
    class NodeReSampler {

    private:
        std::unique_ptr<ReSampler> re_sampler;
        SampleRate output_sample_rate;
        SampleRate input_sample_rate;

    public:

        bool process(AudioFrame &p_audio_frame);

        NodeReSampler(SampleRate p_sample_rate_src,
                      SampleRate p_sample_rate_dst,
                      Channel p_channel,
                      size_t p_sample_buffer_size,
                      double p_req_trans_band);

        virtual ~NodeReSampler() = default;
    };
}

#endif