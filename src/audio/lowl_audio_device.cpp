#include "lowl_audio_device.h"

std::string Lowl::Audio::AudioDevice::get_name() const {
    return name;
}

void Lowl::Audio::AudioDevice::set_name(const std::string &p_name) {
    name = p_name;
}

Lowl::Audio::AudioDevice::AudioDevice() {
    exclusive_mode = false;
    name = std::string();
    audio_source = std::shared_ptr<AudioSource>();
}
