#include "lowl_driver.h"

std::vector<std::shared_ptr<Lowl::Device>> Lowl::Driver::get_devices() const {
    return devices;
}

std::string Lowl::Driver::get_name() const {
    return name;
}

Lowl::Driver::Driver() {
    devices = std::vector<std::shared_ptr<Lowl::Device>>();
    name = std::string("NoDriver");
    default_device = std::shared_ptr<Lowl::Device>();
}

std::shared_ptr<Lowl::Device> Lowl::Driver::get_default_device() const {
    return default_device;
}
