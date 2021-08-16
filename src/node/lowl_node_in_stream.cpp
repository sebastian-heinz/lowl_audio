#include "lowl_node_in_stream.h"

Lowl::NodeInStream::NodeInStream(std::shared_ptr<Lowl::AudioStream> p_stream) {
    stream = p_stream;
}

void Lowl::NodeInStream::process(Lowl::AudioFrame p_audio_frame) {
    AudioSource::ReadResult read_result = stream->read(p_audio_frame);
    if (read_result == AudioSource::ReadResult::Read) {
        output(p_audio_frame);
    }
}
