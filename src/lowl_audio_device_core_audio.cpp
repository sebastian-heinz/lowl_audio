#ifdef LOWL_DRIVER_CORE_AUDIO

#include "lowl_audio_device_core_audio.h"

#include "lowl_logger.h"


void Lowl::AudioDeviceCoreAudio::start(std::shared_ptr<AudioSource> p_audio_source, Lowl::Error &error) {

}

void Lowl::AudioDeviceCoreAudio::stop(Lowl::Error &error) {

}

bool Lowl::AudioDeviceCoreAudio::is_supported(Lowl::AudioChannel channel, Lowl::SampleRate sample_rate,
                                              Lowl::SampleFormat sample_format, Lowl::Error &error) {
    return false;
}

Lowl::SampleRate Lowl::AudioDeviceCoreAudio::get_default_sample_rate() {
    return 0;
}

void Lowl::AudioDeviceCoreAudio::set_exclusive_mode(bool p_exclusive_mode, Lowl::Error &error) {

}

Lowl::AudioDeviceCoreAudio::AudioDeviceCoreAudio() {

}

Lowl::AudioDeviceCoreAudio::~AudioDeviceCoreAudio() {

}

void Lowl::AudioDeviceCoreAudio::set_device_id(AudioObjectID p_device_id) {
    device_id = p_device_id;
}

#endif /* LOWL_DRIVER_CORE_AUDIO */