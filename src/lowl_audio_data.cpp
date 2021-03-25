#include "lowl_audio_data.h"

Lowl::AudioData::AudioData(std::vector<Lowl::AudioFrame> p_audio_frames, SampleRate p_sample_rate,
                           Channel p_channel) {
    sample_rate = p_sample_rate;
    channel = p_channel;
    frames = std::vector<AudioFrame>(p_audio_frames);
    position = 0;
    // do_read = ATOMIC_FLAG_INIT;
    do_read.test_and_set();
}

Lowl::AudioData::~AudioData() {

}

bool Lowl::AudioData::read(Lowl::AudioFrame &audio_frame) {
    if (!do_read.test_and_set()) {
        position = 0;
        return false;
    }
    if (position >= frames.size()) {
        position = 0;
        return false;
    }
    audio_frame = frames[position];
    position++;
    return true;
}

void Lowl::AudioData::cancel_read() {
    do_read.clear();
}

int Lowl::AudioData::get_channel_num() const {
    return Lowl::get_channel_num(channel);
}

Lowl::Channel Lowl::AudioData::get_channel() const {
    return channel;
}

Lowl::SampleRate Lowl::AudioData::get_sample_rate() const {
    return sample_rate;
}

Lowl::SampleFormat Lowl::AudioData::get_sample_format() const {
    return Lowl::SampleFormat::FLOAT_32;
}

std::vector<Lowl::AudioFrame> Lowl::AudioData::get_frames() {
    return std::vector<AudioFrame>(frames);
}