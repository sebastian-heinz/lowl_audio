#include "lowl_audio_driver.h"

std::vector<std::shared_ptr<Lowl::AudioDevice>> Lowl::AudioDriver::get_devices() const {
    return devices;
}

std::string Lowl::AudioDriver::get_name() const {
    return name;
}

Lowl::AudioDriver::AudioDriver() {
    devices = std::vector<std::shared_ptr<Lowl::AudioDevice>>();
    name = std::string("NoDriver");
    default_device = std::shared_ptr<Lowl::AudioDevice>();
}

std::shared_ptr<Lowl::AudioDevice> Lowl::AudioDriver::get_default_device() const {
    return default_device;
}
