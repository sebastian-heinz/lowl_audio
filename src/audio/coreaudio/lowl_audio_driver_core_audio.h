#ifndef LOWL_AUDIO_DRIVER_CORE_AUDIO_H
#define LOWL_AUDIO_DRIVER_CORE_AUDIO_H

#ifdef LOWL_DRIVER_CORE_AUDIO

#include "audio/lowl_audio_driver.h"

#include "lowl_audio_device_core_audio.h"

#include <memory>

#include <CoreAudio/AudioHardware.h>

namespace Lowl {

    class AudioDriverCoreAudio : public AudioDriver {

    private:
        void create_devices(Error &error);

        std::shared_ptr<AudioDeviceCoreAudio> create_device(AudioObjectID p_device_id, Error &error);


        std::string get_device_name(AudioObjectID p_device_id);

        uint32_t get_device_stream_count(AudioObjectID p_device_id, AudioObjectPropertyScope p_scope);

        SampleRate get_device_default_sample_rate(AudioObjectID p_device_id);



    public:
        void initialize(Error &error) override;

        AudioDriverCoreAudio();

        ~AudioDriverCoreAudio() override;
    };
}

#endif /* LOWL_DRIVER_CORE_AUDIO */
#endif /* LOWL_AUDIO_DRIVER_CORE_AUDIO_H */
