#ifndef LOWL_AUDIO_DRIVER_PA_H
#define LOWL_AUDIO_DRIVER_PA_H

#ifdef LOWL_DRIVER_PORTAUDIO

#include "audio/lowl_audio_driver.h"

#include <portaudio.h>

namespace Lowl::Audio {

    class AudioDriverPa : public AudioDriver {

    private:
        void create_devices(Error &error);
        PaHostApiIndex get_default_host_api_index();
        PaDeviceIndex get_default_output_device_index();

    public:
        void initialize(Error &error) override;

        AudioDriverPa();

        ~AudioDriverPa() override;
    };
}

#endif /* LOWL_DRIVER_PORTAUDIO */
#endif /* LOWL_PA_DRIVER_H */
