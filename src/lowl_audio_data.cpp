#include "lowl_audio_data.h"

Lowl::AudioData::AudioData(std::vector<Lowl::AudioFrame> p_audio_frames, SampleRate p_sample_rate, Channel p_channel)
        : AudioSource(p_sample_rate, p_channel) {
    frames = std::vector<AudioFrame>(p_audio_frames);
    position = 0;
    size = frames.size();
}

bool Lowl::AudioData::read(Lowl::AudioFrame &audio_frame) {
    if (position >= size) {
        position = 0;
        return false;
    }
    audio_frame = frames[position];
    process_volume(audio_frame);
    process_panning(audio_frame);
    position++;
    return true;
}

std::vector<Lowl::AudioFrame> Lowl::AudioData::get_frames() {
    return std::vector<AudioFrame>(frames);
}

std::unique_ptr<Lowl::AudioData> Lowl::AudioData::create_slice(double begin_sec, double end_sec) {
    double first_frame = begin_sec * sample_rate;
    double last_frame = end_sec * sample_rate;
    std::vector<AudioFrame> slice;
    if (end_sec > 0.0) {
        slice = std::vector<AudioFrame>(frames.begin() + first_frame, frames.begin() + last_frame);
    }
    else {
        slice = std::vector<AudioFrame>(frames.begin() + first_frame, frames.end());
    }
    return std::make_unique<Lowl::AudioData>(slice, sample_rate, channel);
}

Lowl::size_l Lowl::AudioData::get_frames_remaining() const {
    int remaining = size - position;
    if (remaining < 0) {
        remaining = 0;
    }
    return remaining;
}

Lowl::AudioData::~AudioData() {
}
