#include "lowl_audio_source.h"

#include <cmath>
#include <algorithm>

Lowl::Audio::AudioSource::AudioSource(SampleRate p_sample_rate, AudioChannel p_channel) {
    sample_rate = p_sample_rate;
    channel = p_channel;
    volume.store(DEFAULT_VOLUME);
    panning.store(DEFAULT_PANNING);
    is_playing = true;
}

Lowl::SampleRate Lowl::Audio::AudioSource::get_sample_rate() const {
    return sample_rate;
}

Lowl::Audio::AudioChannel Lowl::Audio::AudioSource::get_channel() const {
    return channel;
}

Lowl::Audio::SampleFormat Lowl::Audio::AudioSource::get_sample_format() const {
    return Lowl::Audio::SampleFormat::FLOAT_32;
}

size_t Lowl::Audio::AudioSource::get_channel_num() const {
    return Lowl::Audio::get_channel_num(channel);
}

void Lowl::Audio::AudioSource::set_volume(Lowl::Volume p_volume) {
    volume.store(p_volume);
}

Lowl::Volume Lowl::Audio::AudioSource::get_volume() {
    return volume.load();
}

void Lowl::Audio::AudioSource::set_panning(Lowl::Panning p_panning) {
    std::clamp(p_panning, MIN_PANNING, MAX_PANNING);
    panning.store(p_panning);
}

Lowl::Panning Lowl::Audio::AudioSource::get_panning() {
    return panning.load();
}

void Lowl::Audio::AudioSource::process_volume(Lowl::Audio::AudioFrame &audio_frame) {
    Volume vol = volume.load();
    for (int current_channel = 0; current_channel < Lowl::Audio::get_channel_num(channel); current_channel++) {
        audio_frame[current_channel] *= vol;
    }
}

void Lowl::Audio::AudioSource::process_panning(Lowl::Audio::AudioFrame &audio_frame) {
    Panning pan = panning.load();
    switch (channel) {
        case Lowl::Audio::AudioChannel::Stereo:
            audio_frame.left *= std::sqrt(1.0 - pan);
            audio_frame.right *= std::sqrt(1.0 + pan);
            break;
        case Lowl::Audio::AudioChannel::Mono:
            audio_frame.left *= std::sqrt(1.0 - pan);
            audio_frame.right *= std::sqrt(1.0 + pan);
            break;
        case Lowl::Audio::AudioChannel::None:
            break;
    }
}

void Lowl::Audio::AudioSource::pause() {
    is_playing = false;
}

bool Lowl::Audio::AudioSource::is_pause() {
    return !is_playing;
}

void Lowl::Audio::AudioSource::play() {
    is_playing = true;
}

bool Lowl::Audio::AudioSource::is_play() {
    return is_playing;
}
