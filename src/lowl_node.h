#ifndef LOWL_NODE_H
#define LOWL_NODE_H

#include "lowl_audio_frame.h"

#include <vector>

namespace Lowl {
    class Node {

    private:
        std::vector<std::shared_ptr<Node>> outputs;

    protected:
        void output(const AudioFrame &p_audio_frame);

    public:
        virtual void process(AudioFrame p_audio_frame) = 0;

        std::shared_ptr<Node> connect(std::shared_ptr<Node> p_node);

        Node();

        virtual ~Node() = default;
    };
}

#endif