#include "lowl_audio_stream.h"

Lowl::AudioStream::AudioStream(SampleRate p_sample_rate, Channel p_channel) {
    sample_rate = p_sample_rate;
    channel = p_channel;
    buffer = new moodycamel::ReaderWriterQueue<AudioFrame>(100);
    frames_in = 0;
    frames_out = 0;
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

Lowl::AudioFrame Lowl::AudioStream::read() {
    AudioFrame frame;
    if (!buffer->try_dequeue(frame)) {
        return frame;
    }
    frames_out++;
    return frame;
}

void Lowl::AudioStream::write(AudioFrame p_audio_frame) {
    if (!buffer->enqueue(p_audio_frame)) {
        return;
    }
    frames_in++;
}

int Lowl::AudioStream::get_channel_num() const {
    return Lowl::get_channel_num(channel);
}

void Lowl::AudioStream::write(const std::vector<AudioFrame> &p_audio_frames) {
    for (AudioFrame frame : p_audio_frames) {
        write(frame);
    }
}

uint32_t Lowl::AudioStream::get_frames_out() const {
    return frames_out;
}

uint32_t Lowl::AudioStream::get_frames_in() const {
    return frames_in;
}
