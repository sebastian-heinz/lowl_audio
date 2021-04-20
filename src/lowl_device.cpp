#include "lowl_device.h"

bool Lowl::Device::is_supported(std::shared_ptr<AudioSource> p_audio_source, Error &error) {
    return is_supported(p_audio_source->get_channel(), p_audio_source->get_sample_rate(),
                        p_audio_source->get_sample_format(), error);
}

std::string Lowl::Device::get_name() const {
    return name;
}

void Lowl::Device::set_name(const std::string &p_name) {
    name = p_name;
}

bool Lowl::Device::is_exclusive_mode() const {
    return exclusive_mode;
}

Lowl::Device::Device() {
    exclusive_mode = false;
    name = std::string();
    audio_source = std::shared_ptr<AudioSource>();
}
