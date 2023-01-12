#include "lowl_audio_device.h"

std::string Lowl::Audio::AudioDevice::get_name() const {
    return name;
}

void Lowl::Audio::AudioDevice::set_name(const std::string &p_name) {
    name = p_name;
}

Lowl::Audio::AudioDevice::AudioDevice(_constructor_tag) {
    properties = std::vector<AudioDeviceProperties>();
    name = std::string();
    audio_source = std::shared_ptr<AudioSource>();
    re_sampler = std::unique_ptr<ReSampler>();
    audio_device_properties = std::unique_ptr<AudioDeviceProperties>();
}

Lowl::Audio::AudioDeviceProperties
Lowl::Audio::AudioDevice::get_closest_properties(Lowl::Audio::AudioDeviceProperties p_audio_device_properties,
                                                 Error &error) const {
    if (properties.empty()) {
        error.set_error(Lowl::ErrorCode::Error);
        return AudioDeviceProperties();
    }
    for (AudioDeviceProperties property: properties) {

    }
    // TODO find best match between `property` and `p_audio_device_properties`
    return properties[0];
}

std::vector<Lowl::Audio::AudioDeviceProperties> Lowl::Audio::AudioDevice::get_properties() const {
    return properties;
}

Lowl::Audio::AudioDevice::~AudioDevice() {

}

void Lowl::Audio::AudioDevice::write_frames(
        void *p_dst,
        unsigned long p_frames_per_buffer,
        unsigned long p_bytes_per_frame) {

    unsigned long current_frame = 0;
    AudioFrame frame{};
    for (; current_frame < p_frames_per_buffer; current_frame++) {
        AudioSource::ReadResult read_result = audio_source->read(frame);
        if (read_result == AudioSource::ReadResult::Read) {
            for (int current_channel = 0; current_channel < audio_source->get_channel_num(); current_channel++) {
                // TODO asset mNumberChannels == audio_device_properties-channels
                Sample sample = std::clamp(
                        frame[current_channel],
                        AudioFrame::MIN_SAMPLE_VALUE,
                        AudioFrame::MAX_SAMPLE_VALUE
                );
                SampleConverter::write_sample(
                        audio_device_properties->sample_format,
                        sample,
                        &p_dst
                );
            }
        } else if (read_result == AudioSource::ReadResult::End) {
            break;
        } else if (read_result == AudioSource::ReadResult::Pause) {
            break;
        } else if (read_result == AudioSource::ReadResult::Remove) {
            break;
        }
    }

    if (current_frame < p_frames_per_buffer) {
        // fill buffer with silence if not enough samples available.
        unsigned long missing_frames = p_frames_per_buffer - current_frame;
        unsigned long missing_samples = missing_frames * (unsigned long) audio_source->get_channel_num();
        unsigned long current_sample = 0;

        uint8_t *remaining = static_cast<uint8_t *>(p_dst);
        for (; current_sample < missing_samples; current_sample++) {
            for (unsigned long frame_byte = 0; frame_byte < p_bytes_per_frame; frame_byte++) {
                *remaining++ = 0;
            }
        }
    }


}
