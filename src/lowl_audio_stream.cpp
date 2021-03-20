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
    // TODO communicate end of user provided frames / potentially end of stream
    AudioFrame frame;
    if (!buffer->try_dequeue(frame)) {
        // if empty return silence
        frame = {};
        return frame;
    }
    frames_out++;
    return frame;
}

void Lowl::AudioStream::write(const AudioFrame &p_audio_frame) {
    if (!buffer->enqueue(p_audio_frame)) {
        return;
    }
    frames_in++;
}

int Lowl::AudioStream::get_channel_num() const {
    return Lowl::get_channel_num(channel);
}

void Lowl::AudioStream::write(const std::vector<AudioFrame> &p_audio_frames) {
    for (const AudioFrame &frame : p_audio_frames) {
        write(frame);
    }
}

uint32_t Lowl::AudioStream::get_num_frame_read() const {
    return frames_out;
}

uint32_t Lowl::AudioStream::get_num_frame_write() const {
    return frames_in;
}

size_t Lowl::AudioStream::get_num_frame_queued() const {
    return buffer->size_approx();
}
