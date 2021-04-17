#ifdef LOWL_DRIVER_DUMMY

#include "lowl_driver_dummy.h"

Lowl::DummyDriver::DummyDriver() {
    name = std::string("DummyDriver");
}

void Lowl::DummyDriver::initialize(Lowl::Error &error) {

}

#endif