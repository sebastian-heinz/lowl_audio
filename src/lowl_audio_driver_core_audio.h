#ifndef LOWL_AUDIO_DRIVER_CORE_AUDIO_H
#define LOWL_AUDIO_DRIVER_CORE_AUDIO_H

#ifdef LOWL_DRIVER_CORE_AUDIO

#include "lowl_audio_driver.h"

#include "lowl_audio_device_core_audio.h"

#include <memory>

#include <CoreAudio/AudioHardware.h>

namespace Lowl {

    class AudioDriverCoreAudio : public AudioDriver {

    private:
        void create_devices(Error &error);
        std::shared_ptr<AudioDeviceCoreAudio> create_device(AudioObjectID device_id, Error &error);

    public:
        void initialize(Error &error) override;

        AudioDriverCoreAudio();

        ~AudioDriverCoreAudio() override;
    };
}

#endif /* LOWL_DRIVER_CORE_AUDIO */
#endif /* LOWL_AUDIO_DRIVER_CORE_AUDIO_H */
