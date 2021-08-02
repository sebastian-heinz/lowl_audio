#include "lowl_audio_data.h"

Lowl::AudioData::AudioData(std::vector<Lowl::AudioFrame> p_audio_frames, SampleRate p_sample_rate, Channel p_channel, Volume p_volume, Panning p_panning)
        : AudioSource(p_sample_rate, p_channel, p_volume, p_panning) {
    frames = std::vector<AudioFrame>(p_audio_frames);
    position = 0;
}

bool Lowl::AudioData::read(Lowl::AudioFrame &audio_frame) {
    if (position >= frames.size()) {
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

std::shared_ptr<Lowl::AudioData> Lowl::AudioData::create_slice(double begin_sec, double end_sec) {
    double first_frame = begin_sec * sample_rate;
    double last_frame = end_sec * sample_rate;
    std::vector<AudioFrame> subvector;
    if (end_sec > 0.0) {
        subvector = std::vector<AudioFrame>(frames.begin() + first_frame, frames.begin() + last_frame);
    }
    else {
        subvector = std::vector<AudioFrame>(frames.begin() + first_frame, frames.end());
    }
    return std::make_shared<Lowl::AudioData>(subvector, sample_rate, channel);
}

Lowl::size_l Lowl::AudioData::get_frames_remaining() const {
    int remaining = frames.size() - position;
    if (remaining < 0) {
        remaining = 0;
    }
    return remaining;
}

Lowl::AudioData::~AudioData() {
 int i = 1;
}
