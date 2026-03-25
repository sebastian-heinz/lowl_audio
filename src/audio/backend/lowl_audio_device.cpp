#include "lowl_audio_device.h"

#include <algorithm>
#include <cstring>

#include "audio/convert/lowl_audio_sample_converter.h"

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
        return AudioDeviceProperties();
    }
    for (AudioDeviceProperties property : properties_list) {
    }
    // TODO find best match between `property` and `p_audio_device_properties`
    return properties_list[0];
}

std::vector<Lowl::Audio::AudioDeviceProperties> Lowl::Audio::AudioDevice::get_properties_list() const {
    return properties_list;
}

Lowl::Audio::AudioDevice::~AudioDevice() {
}

void Lowl::Audio::AudioDevice::allocate_render_buffer(const unsigned long p_frame_capacity) {
    const uint8_t channel_count = static_cast<uint8_t>(get_channel_num(audio_device_properties.channel));
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
