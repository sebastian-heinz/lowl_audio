#include "lowl_audio_source.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include "audio/simd/lowl_audio_simd.h"

Lowl::Audio::AudioSource::AudioSource(const AudioFormat p_audio_format)
    : left_channel_index(p_audio_format.channel_layout.index_of(Speaker::FrontLeft)),
      right_channel_index(p_audio_format.channel_layout.index_of(Speaker::FrontRight)),
      audio_format(p_audio_format) {
    name = std::string();
    gain_cache.local_gains.fill(static_cast<Sample>(1));
}

const Lowl::Audio::AudioFormat &Lowl::Audio::AudioSource::get_audio_format() const {
    return audio_format;
}

uint8_t Lowl::Audio::AudioSource::get_channel_count() const {
    return audio_format.channel_layout.channel_count;
}

void Lowl::Audio::AudioSource::set_volume(Volume p_volume) {
    volume.store(p_volume, std::memory_order_seq_cst);
    gain_generation.fetch_add(1, std::memory_order_release);
}

Lowl::Volume Lowl::Audio::AudioSource::get_volume() const {
    return volume.load(std::memory_order_seq_cst);
}

void Lowl::Audio::AudioSource::set_panning(Panning p_panning) {
    p_panning = std::clamp(p_panning, MIN_PANNING, MAX_PANNING);
    panning.store(p_panning, std::memory_order_seq_cst);
    gain_generation.fetch_add(1, std::memory_order_release);
}

Lowl::Panning Lowl::Audio::AudioSource::get_panning() const {
    return panning.load(std::memory_order_seq_cst);
}

void Lowl::Audio::AudioSource::clear_block(AudioBlockView p_block) {
    for (uint8_t channel_index = 0; channel_index < p_block.channel_count; channel_index++) {
        std::fill_n(p_block.channel(channel_index), p_block.frame_count, static_cast<Sample>(0));
    }
}

void Lowl::Audio::AudioSource::mix_scaled_channel(const Sample *LOWL_RESTRICT p_src,
                                                  Sample *LOWL_RESTRICT p_dst,
                                                  const Sample p_gain,
                                                  const uint32_t p_frame_count) {
    if (p_src == nullptr || p_dst == nullptr || p_frame_count == 0 ||
        std::abs(p_gain) <= std::numeric_limits<Sample>::epsilon()) {
        return;
    }
    Simd::dispatch().mix_scaled_channel(p_src, p_dst, p_gain, p_frame_count);
}

Lowl::Audio::AudioSource::MixGainVector
Lowl::Audio::AudioSource::compose_gain_vector(const MixGainVector &p_upstream_gain) const {
    const uint64_t generation = gain_generation.load(std::memory_order_acquire);
    if (gain_cache.generation != generation) {
        const Volume local_volume = volume.load(std::memory_order_relaxed);
        const Panning local_panning = panning.load(std::memory_order_relaxed);

        gain_cache.local_gains.fill(static_cast<Sample>(local_volume));

        if (left_channel_index >= 0) {
            gain_cache.local_gains[static_cast<size_t>(left_channel_index)] =
                static_cast<Sample>(local_volume * std::sqrt(static_cast<Sample>(1) - local_panning));
        }
        if (right_channel_index >= 0) {
            gain_cache.local_gains[static_cast<size_t>(right_channel_index)] =
                static_cast<Sample>(local_volume * std::sqrt(static_cast<Sample>(1) + local_panning));
        }

        gain_cache.generation = generation;
    }

    MixGainVector composed_gain = make_unity_gain_vector();
    for (uint8_t channel_index = 0; channel_index < audio_format.channel_layout.channel_count; channel_index++) {
        composed_gain[channel_index] =
            p_upstream_gain[channel_index] * gain_cache.local_gains[static_cast<size_t>(channel_index)];
    }
    return composed_gain;
}

Lowl::Audio::AudioSource::MixGainVector Lowl::Audio::AudioSource::make_unity_gain_vector() {
    return MixGainVector{};
}

Lowl::Audio::AudioSource::RenderResult Lowl::Audio::AudioSource::render(AudioBlockView p_block) {
    clear_block(p_block);
    return mix_into(p_block, make_unity_gain_vector());
}

void Lowl::Audio::AudioSource::pause() {
    playback_enabled.store(false, std::memory_order_release);
}

bool Lowl::Audio::AudioSource::is_pause() const {
    return !playback_enabled.load(std::memory_order_acquire);
}

void Lowl::Audio::AudioSource::play() {
    playback_enabled.store(true, std::memory_order_release);
}

bool Lowl::Audio::AudioSource::is_play() const {
    return playback_enabled.load(std::memory_order_acquire);
}

std::string Lowl::Audio::AudioSource::get_name() const {
    std::lock_guard<std::mutex> lock(name_mutex);
    return name;
}

void Lowl::Audio::AudioSource::set_name(const std::string &p_name) {
    std::lock_guard<std::mutex> lock(name_mutex);
    name = p_name;
}
