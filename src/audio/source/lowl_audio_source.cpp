#include "lowl_audio_source.h"

#include <algorithm>
#include <cmath>

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

void Lowl::Audio::AudioSource::on_added_to_mixer() {
}

void Lowl::Audio::AudioSource::on_removed_from_mixer() {
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

void Lowl::Audio::AudioSource::process_volume(AudioBlockView p_block) {
    const Volume vol = volume.load(std::memory_order_relaxed);
    for (uint8_t current_channel = 0; current_channel < p_block.channel_count; current_channel++) {
        Sample *channel_data = p_block.channel(current_channel);
        for (uint32_t frame_index = 0; frame_index < p_block.frame_count; frame_index++) {
            channel_data[frame_index] *= vol;
        }
    }
}

void Lowl::Audio::AudioSource::process_panning(AudioBlockView p_block) {
    const Panning pan = panning.load(std::memory_order_relaxed);
    if (p_block.channel_count == 0) {
        return;
    }

    const Sample gain_l = static_cast<Sample>(std::sqrt(1.0 - pan));
    Sample *left = p_block.channel(0);
    for (uint32_t frame_index = 0; frame_index < p_block.frame_count; frame_index++) {
        left[frame_index] *= gain_l;
    }

    if (p_block.channel_count < 2) {
        return;
    }

    const Sample gain_r = static_cast<Sample>(std::sqrt(1.0 + pan));
    Sample *right = p_block.channel(1);
    for (uint32_t frame_index = 0; frame_index < p_block.frame_count; frame_index++) {
        right[frame_index] *= gain_r;
    }
}

void Lowl::Audio::AudioSource::pause() {
    playback_enabled.store(false, std::memory_order_relaxed);
}

bool Lowl::Audio::AudioSource::is_pause() const {
    return !playback_enabled.load(std::memory_order_relaxed);
}

void Lowl::Audio::AudioSource::play() {
    playback_enabled.store(true, std::memory_order_relaxed);
}

bool Lowl::Audio::AudioSource::is_play() {
    return playback_enabled.load(std::memory_order_relaxed);
}

std::string Lowl::Audio::AudioSource::get_name() const {
    return name;
}

void Lowl::Audio::AudioSource::set_name(const std::string &p_name) {
    name = p_name;
}
