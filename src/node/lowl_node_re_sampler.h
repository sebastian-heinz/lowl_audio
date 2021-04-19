#ifndef LOWL_NODE_RE_SAMPLER_H
#define LOWL_NODE_RE_SAMPLER_H

#include "../lowl_audio_frame.h"
#include "../lowl_re_sampler.h"
#include "lowl_node.h"

#include <memory>

namespace Lowl {
    class NodeReSampler : public Node {

    private:
        std::unique_ptr<ReSampler> re_sampler;
        SampleRate output_sample_rate;
        SampleRate input_sample_rate;

    public:
        void process(AudioFrame p_audio_frame) override;

        NodeReSampler(SampleRate p_sample_rate_src,
                      SampleRate p_sample_rate_dst,
                      Channel p_channel,
                      size_t p_sample_buffer_size,
                      double p_req_trans_band);

        virtual ~NodeReSampler() = default;
    };
}

#endif