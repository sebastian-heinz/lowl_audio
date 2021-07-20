#include "lowl_audio_source.h"

#include <cmath>

Lowl::AudioSource::AudioSource(SampleRate p_sample_rate, Channel p_channel, Volume p_volume, Panning p_panning) {
    sample_rate = p_sample_rate;
    channel = p_channel;
    volume.store(p_volume);
    panning.store(p_panning);
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
            audio_frame.right *= std::sqrt(pan);
            break;
        case Lowl::Channel::Mono:
            audio_frame.left *= std::sqrt(1.0 - pan);
            audio_frame.right = audio_frame.left * std::sqrt(pan);
            break;
    }
}