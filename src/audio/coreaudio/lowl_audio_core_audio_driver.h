#ifndef LOWL_AUDIO_DRIVER_CORE_AUDIO_H
#define LOWL_AUDIO_DRIVER_CORE_AUDIO_H

#ifdef LOWL_DRIVER_CORE_AUDIO

#include "audio/lowl_audio_driver.h"

#include "lowl_audio_core_audio_device.h"

#include <memory>

#include <CoreAudio/AudioHardware.h>

namespace Lowl::Audio {

    class CoreAudioDriver : public AudioDriver {

    private:
        void create_devices(Error &error);

        std::shared_ptr<CoreAudioDevice> create_device(AudioObjectID p_device_id, Error &error);

    public:
        void initialize(Error &error) override;

        CoreAudioDriver();

        ~CoreAudioDriver() override;
    };
}

#endif /* LOWL_DRIVER_CORE_AUDIO */
#endif /* LOWL_AUDIO_DRIVER_CORE_AUDIO_H */
