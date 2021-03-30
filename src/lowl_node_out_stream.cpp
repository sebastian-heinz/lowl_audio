#include "lowl_node_out_stream.h"

Lowl::NodeOutStream::NodeOutStream(SampleRate p_sample_rate, Channel p_channel) {
    stream = std::make_shared<AudioStream>(p_sample_rate, p_channel);
}

void Lowl::NodeOutStream::process(Lowl::AudioFrame p_audio_frame) {
    stream->write(p_audio_frame);
}

std::shared_ptr<Lowl::AudioStream> Lowl::NodeOutStream::get_stream() {
    return stream;
}
