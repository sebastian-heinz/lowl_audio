#ifndef LOWL_AUDIO_CORE_AUDIO_DRIVER_H
#define LOWL_AUDIO_CORE_AUDIO_DRIVER_H

#ifdef LOWL_DRIVER_CORE_AUDIO

#include <CoreAudio/AudioHardware.h>

#include <memory>

#include "audio/backend/coreaudio/lowl_audio_core_audio_device.h"
#include "audio/backend/lowl_audio_driver.h"

namespace Lowl::Audio {

    class CoreAudioDriver : public AudioDriver {

    private:
        void create_devices(Error &error);

    public:
        void initialize(Error &error) override;

        CoreAudioDriver();

        ~CoreAudioDriver() override;
    };
} // namespace Lowl::Audio

#endif /* LOWL_DRIVER_CORE_AUDIO */
#endif /* LOWL_AUDIO_CORE_AUDIO_DRIVER_H */
