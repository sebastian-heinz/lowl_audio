#include "lowl_audio_device.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <tuple>

#include "audio/convert/lowl_audio_sample_converter.h"

namespace {
    auto make_property_score(const Lowl::Audio::AudioDeviceProperties &p_requested,
                             const Lowl::Audio::AudioDeviceProperties &p_candidate) {
        const bool layout_mismatch =
            p_requested.channel_layout.is_valid() && p_candidate.channel_layout != p_requested.channel_layout;
        const bool format_mismatch = p_requested.sample_format != Lowl::Audio::SampleFormat::Unknown &&
                                     p_candidate.sample_format != p_requested.sample_format;
        const double sample_rate_distance =
            p_requested.sample_rate > Lowl::NO_SAMPLE_RATE &&
                    !Lowl::Audio::sample_rates_equal(p_requested.sample_rate, p_candidate.sample_rate)
                ? std::abs(p_candidate.sample_rate - p_requested.sample_rate)
                : 0.0;

        return std::make_tuple(!p_candidate.is_supported,
                               layout_mismatch,
                               format_mismatch,
                               p_candidate.exclusive_mode != p_requested.exclusive_mode,
                               sample_rate_distance);
    }
} // namespace

std::string Lowl::Audio::AudioDevice::get_name() const {
    return name;
}

void Lowl::Audio::AudioDevice::set_name(const std::string &p_name) {
    name = p_name;
}

Lowl::Audio::AudioDevice::AudioDevice(_constructor_tag) {
    properties_list = std::vector<AudioDeviceProperties>();
    name = std::string();
    audio_source = std::shared_ptr<AudioSource>();
    audio_device_properties = AudioDeviceProperties{};
    render_buffer = nullptr;
}

Lowl::Audio::AudioDeviceProperties
Lowl::Audio::AudioDevice::get_closest_properties(AudioDeviceProperties p_audio_device_properties, Error &error) const {
    if (properties_list.empty()) {
        error.set_error(ErrorCode::DeviceHasNoAudioProperties);
        return AudioDeviceProperties{};
    }

    auto best_property = properties_list.begin();
    auto best_score = make_property_score(p_audio_device_properties, *best_property);
    for (auto property = std::next(properties_list.begin()); property != properties_list.end(); ++property) {
        const auto property_score = make_property_score(p_audio_device_properties, *property);
        if (property_score < best_score) {
            best_property = property;
            best_score = property_score;
        }
    }
    return *best_property;
}

std::vector<Lowl::Audio::AudioDeviceProperties> Lowl::Audio::AudioDevice::get_properties_list() const {
    return properties_list;
}

Lowl::Audio::AudioDevice::~AudioDevice() {
}

void Lowl::Audio::AudioDevice::allocate_render_buffer(const unsigned long p_frame_capacity) {
    const uint8_t channel_count = audio_device_properties.channel_layout.channel_count;
    render_buffer = std::make_unique<AudioBuffer>(static_cast<uint32_t>(p_frame_capacity), channel_count);
}

void Lowl::Audio::AudioDevice::render_to_device_buffer(void *p_dst,
                                                       unsigned long p_frames_per_buffer,
                                                       unsigned long p_bytes_per_frame) {
    if (p_dst == nullptr) {
        return;
    }

    if (!audio_source || !render_buffer) {
        std::memset(p_dst, 0, static_cast<size_t>(p_frames_per_buffer) * p_bytes_per_frame);
        return;
    }

    AudioBlockView output_block = render_buffer->view(static_cast<uint32_t>(p_frames_per_buffer));
    render_buffer->clear(output_block.frame_count);
    AudioSource::RenderResult render_result = audio_source->render(output_block);
    const uint32_t produced_frames = std::min(render_result.frames_produced, output_block.frame_count);

    void *write_ptr = p_dst;
    for (uint32_t frame_index = 0; frame_index < produced_frames; frame_index++) {
        for (uint8_t channel_index = 0; channel_index < output_block.channel_count; channel_index++) {
            const Sample sample = std::clamp(
                output_block.channel(channel_index)[frame_index], static_cast<Sample>(-1.0), static_cast<Sample>(1.0));
            SampleConverter::write_sample(audio_device_properties.sample_format, sample, &write_ptr);
        }
    }

    if (produced_frames < p_frames_per_buffer) {
        const unsigned long missing_frames = p_frames_per_buffer - produced_frames;
        std::memset(write_ptr, 0, static_cast<size_t>(missing_frames) * p_bytes_per_frame);
    }
}
