#ifdef LOWL_DRIVER_DUMMY

#include "lowl_dummy_driver.h"

LowlDummyDriver::LowlDummyDriver() {
    name = std::string("DummyDriver");
}

void LowlDummyDriver::initialize(LowlError &error) {

}

LowlDummyDriver::~LowlDummyDriver() {

}

#endif