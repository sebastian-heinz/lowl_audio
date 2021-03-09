#include "../include/lowl_device.h"

std::string LowlDevice::get_name() const
{
	return name;
}

void LowlDevice::set_name(const std::string& p_name)
{
	name = p_name;
}

LowlDevice::~LowlDevice() {

}

LowlDevice::LowlDevice() {

}
