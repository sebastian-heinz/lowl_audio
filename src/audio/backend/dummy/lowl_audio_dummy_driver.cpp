#ifdef LOWL_DRIVER_DUMMY

#include "lowl_audio_dummy_driver.h"

Lowl::Audio::AudioDriverDummy::AudioDriverDummy() : AudioDriver() {
    name = std::string("DummyDriver");
}

void Lowl::Audio::AudioDriverDummy::initialize(Lowl::Error &error) {

}

#endif