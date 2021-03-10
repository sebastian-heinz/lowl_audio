#include "lowl_audio_stream.h"

Lowl::AudioStream::AudioStream(SampleRate p_sample_rate, Channel p_channel) {
    sample_rate = p_sample_rate;
    channel = p_channel;
    buffer = new moodycamel::ReaderWriterQueue<AudioFrame>(100);
}

Lowl::AudioStream::~AudioStream() {
    delete buffer;
}

Lowl::Channel Lowl::AudioStream::get_channel() const {
    return channel;
}

Lowl::SampleRate Lowl::AudioStream::get_sample_rate() const {
    return sample_rate;
}

Lowl::SampleFormat Lowl::AudioStream::get_sample_format() const {
    return SampleFormat::FLOAT_32;
}

Lowl::AudioFrame Lowl::AudioStream::read() const {
    AudioFrame frame;
    if (!buffer->try_dequeue(frame)) {
        return frame;
    }
    return frame;
}

void Lowl::AudioStream::write(AudioFrame p_audio_frame) {
    if (!buffer->enqueue(p_audio_frame)) {
        return;
    }
}

int Lowl::AudioStream::get_channel_num() const {
    return Lowl::get_channel_num(channel);
}