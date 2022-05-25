#ifndef LOWL_AUDIO_CORE_AUDIO_DRIVER_H
#define LOWL_AUDIO_CORE_AUDIO_DRIVER_H

#ifdef LOWL_DRIVER_CORE_AUDIO

#include "audio/lowl_audio_driver.h"

#include "audio/coreaudio/lowl_audio_core_audio_device.h"

#include <memory>

#include <CoreAudio/AudioHardware.h>

namespace Lowl::Audio {

    class CoreAudioDriver : public AudioDriver {

    private:
        void create_devices(Error &error);

    public:
        void initialize(Error &error) override;

        CoreAudioDriver();

        ~CoreAudioDriver() override;
    };
}

#endif /* LOWL_DRIVER_CORE_AUDIO */
#endif /* LOWL_AUDIO_CORE_AUDIO_DRIVER_H */
