#include "lowl_audio_data.h"

Lowl::AudioData::AudioData(SampleRate p_sample_rate, Channel p_channel) {
    sample_rate = p_sample_rate;
    channel = p_channel;
    frames = std::vector<AudioFrame>();
    position = 0;
}

Lowl::AudioData::~AudioData() {

}

bool Lowl::AudioData::write(const std::vector<Lowl::AudioFrame> &p_audio_frames) {
    frames.insert(std::end(frames), std::begin(p_audio_frames), std::end(p_audio_frames));
    return true;
}

bool Lowl::AudioData::write(const Lowl::AudioFrame &p_audio_frame) {
    frames.push_back(p_audio_frame);
    return true;
}

bool Lowl::AudioData::read(Lowl::AudioFrame &audio_frame) {
    if (position >= frames.size()) {
        position = 0;
        return false;
    }
    audio_frame = frames[position];
    position++;
    return true;
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