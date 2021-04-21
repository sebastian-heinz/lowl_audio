#include "lowl_audio_data.h"

Lowl::AudioData::AudioData(std::vector<Lowl::AudioFrame> p_audio_frames, SampleRate p_sample_rate, Channel p_channel)
        : AudioSource(p_sample_rate, p_channel) {
    frames = std::vector<AudioFrame>(p_audio_frames);
    position = 0;
    is_not_cancel.test_and_set();
    is_not_reset.test_and_set();
}

bool Lowl::AudioData::read(Lowl::AudioFrame &audio_frame) {
    if (!is_not_reset.test_and_set()) {
        position = 0;
        return true;
    }
    if (!is_not_cancel.test_and_set()) {
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
    is_not_cancel.clear();
}

void Lowl::AudioData::reset_read() {
    is_not_reset.clear();
}

std::vector<Lowl::AudioFrame> Lowl::AudioData::get_frames() {
    return std::vector<AudioFrame>(frames);
}

std::shared_ptr<Lowl::AudioData> Lowl::AudioData::create_slice(double begin_sec, double end_sec) {
    double first_frame = begin_sec * sample_rate;
    double last_frame = end_sec * sample_rate;
    std::vector<AudioFrame> subvector = std::vector<AudioFrame>(frames.begin() + first_frame, frames.begin() + last_frame);
    return std::make_shared<Lowl::AudioData>(subvector, sample_rate, channel);
}

bool Lowl::AudioData::is_in_mixer() const {
    return in_mixer;
}

void Lowl::AudioData::set_in_mixer(bool p_in_mixer) {
    in_mixer = p_in_mixer;
}

Lowl::size_l Lowl::AudioData::get_frames_remaining() const {
    int remaining = frames.size() - position;
    if (remaining < 0) {
        remaining = 0;
    }
    return remaining;
}
