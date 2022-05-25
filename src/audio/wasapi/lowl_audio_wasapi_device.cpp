#ifdef LOWL_DRIVER_WASAPI

#include "lowl_audio_wasapi_device.h"


Lowl::Audio::WasapiDevice::WasapiDevice() {
}

Lowl::Audio::WasapiDevice::~WasapiDevice() {
}

void Lowl::Audio::WasapiDevice::start(std::shared_ptr<AudioSource> p_audio_source, Lowl::Error &error) {

}

void Lowl::Audio::WasapiDevice::stop(Lowl::Error &error) {

}

bool Lowl::Audio::WasapiDevice::is_supported(Lowl::Audio::AudioChannel channel, Lowl::SampleRate sample_rate,
                                             Lowl::Audio::SampleFormat sample_format, Lowl::Error &error) {
    return false;
}

Lowl::SampleRate Lowl::Audio::WasapiDevice::get_default_sample_rate() {
    return 0;
}

void Lowl::Audio::WasapiDevice::set_exclusive_mode(bool p_exclusive_mode, Lowl::Error &error) {

}

#endif
