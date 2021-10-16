#include "lowl_audio_device.h"

bool Lowl::AudioDevice::is_supported(std::shared_ptr<AudioSource> p_audio_source, Error &error) {
    return is_supported(p_audio_source->get_channel(), p_audio_source->get_sample_rate(),
                        p_audio_source->get_sample_format(), error);
}

std::string Lowl::AudioDevice::get_name() const {
    return name;
}

void Lowl::AudioDevice::set_name(const std::string &p_name) {
    name = p_name;
}

bool Lowl::AudioDevice::is_exclusive_mode() const {
    return exclusive_mode;
}

Lowl::AudioDevice::AudioDevice() {
    exclusive_mode = false;
    name = std::string();
    audio_source = std::shared_ptr<AudioSource>();
}