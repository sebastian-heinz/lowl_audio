#include "lowl_audio_source.h"

#include <cmath>
#include <algorithm>

Lowl::AudioSource::AudioSource(SampleRate p_sample_rate, Channel p_channel) {
    sample_rate = p_sample_rate;
    channel = p_channel;
    volume.store(DEFAULT_VOLUME);
    panning.store(DEFAULT_PANNING);
    is_playing = true;
}

Lowl::SampleRate Lowl::AudioSource::get_sample_rate() const {
    return sample_rate;
}

Lowl::Channel Lowl::AudioSource::get_channel() const {
    return channel;
}

Lowl::SampleFormat Lowl::AudioSource::get_sample_format() const {
    return Lowl::SampleFormat::FLOAT_32;
}

int Lowl::AudioSource::get_channel_num() const {
    return Lowl::get_channel_num(channel);
}

void Lowl::AudioSource::set_volume(Lowl::Volume p_volume) {
    volume.store(p_volume);
}

Lowl::Volume Lowl::AudioSource::get_volume() {
    return volume.load();
}

void Lowl::AudioSource::set_panning(Lowl::Panning p_panning) {
    std::clamp(p_panning, MIN_PANNING, MAX_PANNING);
    panning.store(p_panning);
}

Lowl::Panning Lowl::AudioSource::get_panning() {
    return panning.load();
}

void Lowl::AudioSource::process_volume(Lowl::AudioFrame &audio_frame) {
    Volume vol = volume.load();
    for (int current_channel = 0; current_channel < Lowl::get_channel_num(channel); current_channel++) {
        audio_frame[current_channel] *= vol;
    }
}

void Lowl::AudioSource::process_panning(Lowl::AudioFrame &audio_frame) {
    Panning pan = panning.load();
    switch (channel) {
        case Lowl::Channel::Stereo:
            audio_frame.left *= std::sqrt(1.0 - pan);
            audio_frame.right *= std::sqrt(1.0 + pan);
            break;
        case Lowl::Channel::Mono:
            audio_frame.left *= std::sqrt(1.0 - pan);
            audio_frame.right *= std::sqrt(1.0 + pan);
            break;
    }
}

void Lowl::AudioSource::pause() {
    is_playing = false;
}

bool Lowl::AudioSource::is_pause() {
    return !is_playing;
}

void Lowl::AudioSource::play() {
    is_playing = true;
}

bool Lowl::AudioSource::is_play() {
    return is_playing;
}
