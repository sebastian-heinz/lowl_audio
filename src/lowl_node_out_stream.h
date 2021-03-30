#ifndef LOWL_NODE_OUT_STREAM_H
#define LOWL_NODE_OUT_STREAM_H

#include "lowl_node.h"
#include "lowl_audio_frame.h"
#include "lowl_audio_stream.h"

#include <memory>

namespace Lowl {
    class NodeOutStream : public Node {
    private:
        std::shared_ptr<Lowl::AudioStream> stream;

    public:
        void process(AudioFrame p_audio_frame) override;

        std::shared_ptr<Lowl::AudioStream> get_stream();

        NodeOutStream(SampleRate p_sample_rate, Channel p_channel);

    };
}

#endif