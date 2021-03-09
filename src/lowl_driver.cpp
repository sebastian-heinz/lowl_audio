#include "../include/lowl_driver.h"

std::vector<LowlDevice*> LowlDriver::get_devices() const
{
	return devices;
}

std::string LowlDriver::get_name() const
{
	return name;
}

LowlDriver::LowlDriver() {
	devices = std::vector<LowlDevice*>();
	name = std::string("NoDriver");
}

LowlDriver::~LowlDriver() {
}
