#include "lowl_audio_driver.h"

std::vector<std::shared_ptr<Lowl::Audio::AudioDevice>> Lowl::Audio::AudioDriver::get_devices() const {
    return devices;
}

std::string Lowl::Audio::AudioDriver::get_name() const {
    return name;
}

Lowl::Audio::AudioDriver::AudioDriver() {
    devices = std::vector<std::shared_ptr<Lowl::Audio::AudioDevice>>();
    name = std::string("NoDriver");
    default_device = std::shared_ptr<Lowl::Audio::AudioDevice>();
}

std::shared_ptr<Lowl::Audio::AudioDevice> Lowl::Audio::AudioDriver::get_default_device() const {
    return default_device;
}
