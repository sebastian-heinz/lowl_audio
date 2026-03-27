#include "lowl_audio_source.h"

#include <algorithm>
#include <cmath>
#include <limits>

Lowl::Audio::AudioSource::AudioSource(const SampleRate p_sample_rate, const ChannelLayout p_channel_layout)
    : sample_rate(p_sample_rate), channel_layout(p_channel_layout) {
    name = std::string();
}

Lowl::SampleRate Lowl::Audio::AudioSource::get_sample_rate() const {
    return sample_rate;
}

Lowl::Audio::ChannelLayout Lowl::Audio::AudioSource::get_channel_layout() const {
    return channel_layout;
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
    properties.channel_layout = get_channel_layout();
    properties.sample_format = get_sample_format();
    properties.sample_rate = get_sample_rate();
    return properties;
}

uint8_t Lowl::Audio::AudioSource::get_channel_count() const {
    return channel_layout.channel_count;
}

void Lowl::Audio::AudioSource::set_volume(Volume p_volume) {
    volume.store(p_volume, std::memory_order_seq_cst);
}

Lowl::Volume Lowl::Audio::AudioSource::get_volume() const {
    return volume.load(std::memory_order_seq_cst);
}

void Lowl::Audio::AudioSource::set_panning(Panning p_panning) {
    p_panning = std::clamp(p_panning, MIN_PANNING, MAX_PANNING);
    panning.store(p_panning);
}

Lowl::Panning Lowl::Audio::AudioSource::get_panning() const {
    return panning.load(std::memory_order_seq_cst);
}

void Lowl::Audio::AudioSource::process_volume(AudioBlockView p_block) const {
    const Volume vol = volume.load(std::memory_order_relaxed);
    for (uint8_t current_channel = 0; current_channel < p_block.channel_count; current_channel++) {
        Sample *channel_data = p_block.channel(current_channel);
        for (uint32_t frame_index = 0; frame_index < p_block.frame_count; frame_index++) {
            channel_data[frame_index] *= vol;
        }
    }
}

void Lowl::Audio::AudioSource::process_panning(AudioBlockView p_block) const {
    const Panning pan = panning.load(std::memory_order_relaxed);
    if (std::abs(pan - DEFAULT_PANNING) <= std::numeric_limits<Volume>::epsilon() || p_block.channel_count == 0) {
        return;
    }

    const Volume clamped = std::clamp(pan, static_cast<Volume>(-1), static_cast<Volume>(1));
    const int left_index = channel_layout.index_of(Speaker::FrontLeft);
    const int right_index = channel_layout.index_of(Speaker::FrontRight);

    if (left_index >= 0) {
        const Volume left_gain = static_cast<Volume>(std::sqrt(static_cast<Sample>(1) - clamped));
        Sample *left = p_block.channel(static_cast<uint8_t>(left_index));
        for (uint32_t frame_index = 0; frame_index < p_block.frame_count; frame_index++) {
            left[frame_index] *= left_gain;
        }
    }

    if (right_index >= 0) {
        const Volume right_gain = static_cast<Volume>(std::sqrt(static_cast<Sample>(1) + clamped));
        Sample *right = p_block.channel(static_cast<uint8_t>(right_index));
        for (uint32_t frame_index = 0; frame_index < p_block.frame_count; frame_index++) {
            right[frame_index] *= right_gain;
        }
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

bool Lowl::Audio::AudioSource::is_play() const {
    return playback_enabled.load(std::memory_order_relaxed);
}

std::string Lowl::Audio::AudioSource::get_name() const {
    std::lock_guard<std::mutex> lock(name_mutex);
    return name;
}

void Lowl::Audio::AudioSource::set_name(const std::string &p_name) {
    std::lock_guard<std::mutex> lock(name_mutex);
    name = p_name;
}
