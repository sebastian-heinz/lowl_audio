#ifdef LOWL_DRIVER_CORE_AUDIO

#include "lowl_audio_core_audio_device.h"

#include "lowl_logger.h"


void Lowl::Audio::CoreAudioDevice::start(std::shared_ptr<AudioSource> p_audio_source, Lowl::Error &error) {

}

void Lowl::Audio::CoreAudioDevice::stop(Lowl::Error &error) {

}

bool Lowl::Audio::CoreAudioDevice::is_supported(Lowl::AudioChannel channel, Lowl::SampleRate sample_rate,
                                                Lowl::SampleFormat sample_format, Lowl::Error &error) {
    return false;
}

Lowl::SampleRate Lowl::Audio::CoreAudioDevice::get_default_sample_rate() {
    return 0;
}

void Lowl::Audio::CoreAudioDevice::set_exclusive_mode(bool p_exclusive_mode, Lowl::Error &error) {

}

Lowl::Audio::CoreAudioDevice::CoreAudioDevice() {

}

Lowl::Audio::CoreAudioDevice::~CoreAudioDevice() {

}

void Lowl::Audio::CoreAudioDevice::set_device_id(AudioObjectID p_device_id) {
    device_id = p_device_id;
}

#endif /* LOWL_DRIVER_CORE_AUDIO */