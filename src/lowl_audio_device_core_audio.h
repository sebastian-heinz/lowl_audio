#ifndef LOWL_AUDIO_DEVICE_CORE_AUDIO_H
#define LOWL_AUDIO_DEVICE_CORE_AUDIO_H

#ifdef LOWL_DRIVER_CORE_AUDIO

#include "lowl_audio_device.h"

#include <CoreAudio/AudioHardware.h>

namespace Lowl {

    class AudioDeviceCoreAudio : public AudioDevice {

    private:
        AudioObjectID device_id;

    public:
        void start(std::shared_ptr<AudioSource> p_audio_source, Error &error) override;

        void stop(Error &error) override;

        bool is_supported(Lowl::AudioChannel channel, Lowl::SampleRate sample_rate, Lowl::SampleFormat sample_format,
                          Error &error) override;

        Lowl::SampleRate get_default_sample_rate() override;

        void set_exclusive_mode(bool p_exclusive_mode, Error &error) override;

        void set_device_id(AudioObjectID p_device_id);

        AudioDeviceCoreAudio();

        ~AudioDeviceCoreAudio();
    };
}

#endif /* LOWL_DRIVER_CORE_AUDIO */
#endif /* LOWL_AUDIO_DEVICE_CORE_AUDIO_H */
