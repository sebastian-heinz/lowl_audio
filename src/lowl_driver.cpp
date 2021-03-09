#include "lowl_driver.h"
#include "lowl_device.h"

std::vector<Lowl::Device *> Lowl::Driver::get_devices() const {
    return devices;
}

std::string Lowl::Driver::get_name() const {
    return name;
}

Lowl::Driver::Driver() {
    devices = std::vector<Lowl::Device *>();
    name = std::string("NoDriver");
}