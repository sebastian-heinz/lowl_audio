#include "lowl_audio_source.h"

#include <cmath>
#include <algorithm>

Lowl::Audio::AudioSource::AudioSource(const SampleRate p_sample_rate, const AudioChannel p_channel) {
    sample_rate = p_sample_rate;
    channel = p_channel;
    volume.store(DEFAULT_VOLUME);
    panning.store(DEFAULT_PANNING);
    name = std::string();
}

Lowl::SampleRate Lowl::Audio::AudioSource::get_sample_rate() const {
    return sample_rate;
}

Lowl::Audio::AudioChannel Lowl::Audio::AudioSource::get_channel() const {
    return channel;
}

Lowl::Audio::SampleFormat Lowl::Audio::AudioSource::get_sample_format() const {
    return SampleFormat::FLOAT_32;
}


Lowl::Audio::AudioDeviceProperties Lowl::Audio::AudioSource::get_properties() const {
    AudioDeviceProperties properties{};
    properties.exclusive_mode = false;
    properties.channel = get_channel();
    properties.sample_format = get_sample_format();
    properties.sample_rate = get_sample_rate();
    return properties;
}


size_t Lowl::Audio::AudioSource::get_channel_num() const {
    return Audio::get_channel_num(channel);
}

void Lowl::Audio::AudioSource::set_volume(Volume p_volume) {
    volume.store(p_volume, std::memory_order_seq_cst);
}

Lowl::Volume Lowl::Audio::AudioSource::get_volume() {
    return volume.load(std::memory_order_seq_cst);
}

void Lowl::Audio::AudioSource::set_panning(Panning p_panning) {
    p_panning = std::clamp(p_panning, MIN_PANNING, MAX_PANNING);
    panning.store(p_panning);
}

Lowl::Panning Lowl::Audio::AudioSource::get_panning() {
    return panning.load(std::memory_order_seq_cst);
}

void Lowl::Audio::AudioSource::process_volume(AudioFrame &audio_frame) {
    const Volume vol = volume.load(std::memory_order_relaxed);
    for (int current_channel = 0; current_channel < Audio::get_channel_num(channel); current_channel++) {
        audio_frame[current_channel] *= vol;
    }
}

void Lowl::Audio::AudioSource::process_panning(AudioFrame &audio_frame) {
    const Panning pan = panning.load(std::memory_order_relaxed);
    switch (channel) {
        case AudioChannel::Quadraphonic:
            // TODO
            audio_frame.left *= static_cast<Sample>(std::sqrt(1.0 - pan));
            audio_frame.right *= static_cast<Sample>(std::sqrt(1.0 + pan));
            break;
        case AudioChannel::Stereo:
            audio_frame.left *= static_cast<Sample>(std::sqrt(1.0 - pan));
            audio_frame.right *= static_cast<Sample>(std::sqrt(1.0 + pan));
            break;
        case AudioChannel::Mono:
            audio_frame.left *= static_cast<Sample>(std::sqrt(1.0 - pan));
            audio_frame.right *= static_cast<Sample>(std::sqrt(1.0 + pan));
            break;
        case AudioChannel::None:
            break;
    }
}

void Lowl::Audio::AudioSource::pause() {
    is_playing.store(false, std::memory_order_seq_cst);
}

bool Lowl::Audio::AudioSource::is_pause() const {
    return !is_playing.load(std::memory_order_seq_cst);
}

void Lowl::Audio::AudioSource::play() {
    is_playing.store(true, std::memory_order_seq_cst);
}

bool Lowl::Audio::AudioSource::is_play() {
    return is_playing.load(std::memory_order_seq_cst);
}

std::string Lowl::Audio::AudioSource::get_name() const {
    return name;
}

void Lowl::Audio::AudioSource::set_name(const std::string &p_name) {
    name = p_name;
}
