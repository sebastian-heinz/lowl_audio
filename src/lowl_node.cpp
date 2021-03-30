#include "lowl_node.h"

Lowl::Node::Node() {
    outputs = std::vector<std::shared_ptr<Node>>();
}

std::shared_ptr<Lowl::Node> Lowl::Node::connect(std::shared_ptr<Node> p_node) {
    outputs.push_back(p_node);
    return p_node;
}

void Lowl::Node::output(const AudioFrame &p_audio_frame) {
    for (std::shared_ptr<Node> output : outputs) {
        output->process(p_audio_frame);
    }
}
