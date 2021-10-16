#ifdef LOWL_DRIVER_DUMMY

#include "lowl_audio_driver_dummy.h"

Lowl::AudioDriverDummy::AudioDriverDummy() : AudioDriver() {
    name = std::string("DummyDriver");
}

void Lowl::AudioDriverDummy::initialize(Lowl::Error &error) {

}

#endif