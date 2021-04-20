#ifndef LOWL_NODE_IN_STREAM_H
#define LOWL_NODE_IN_STREAM_H

#include "lowl_node.h"
#include "../lowl_audio_frame.h"
#include "../lowl_audio_stream.h"

#include <memory>

namespace Lowl {
    class NodeInStream : public Node {

    private:
        std::shared_ptr<Lowl::AudioStream> stream;

    public:
        void process(AudioFrame p_audio_frame) override;
        NodeInStream(std::shared_ptr<Lowl::AudioStream> p_stream);
    };
}

#endif