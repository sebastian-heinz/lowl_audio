#include "lowl_device.h"

std::string Lowl::Device::get_name() const {
    return name;
}

void Lowl::Device::set_name(const std::string &p_name) {
    name = p_name;
}