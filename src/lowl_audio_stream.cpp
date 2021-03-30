#include "lowl_audio_stream.h"

Lowl::AudioStream::AudioStream(SampleRate p_sample_rate, Channel p_channel) {
    sample_rate = p_sample_rate;
    channel = p_channel;
    frame_queue = new moodycamel::ReaderWriterQueue<AudioFrame>(100);
    frames_in = 0;
    frames_out = 0;
}

Lowl::AudioStream::~AudioStream() {
    delete frame_queue;
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

bool Lowl::AudioStream::read(AudioFrame &audio_frame) {
    if (!frame_queue->try_dequeue(audio_frame)) {
        return false;
    }
    frames_out++;
    return true;
}

bool Lowl::AudioStream::write(const AudioFrame &p_audio_frame) {
    if (!frame_queue->enqueue(p_audio_frame)) {
        return false;
    }
    frames_in++;
    return true;
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
    return frame_queue->size_approx();
}

std::vector<Lowl::AudioFrame> Lowl::AudioStream::read_all() {
    std::vector<AudioFrame> frames = std::vector<AudioFrame>();
    AudioFrame frame;
    while (read(frame)) {
        frames.push_back(frame);
    }
    return frames;
}