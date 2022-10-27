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