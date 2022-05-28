#ifndef LOWL_AUDIO_WASAPI_DRIVER_H
#define LOWL_AUDIO_WASAPI_DRIVER_H

#ifdef LOWL_DRIVER_WASAPI

#include "audio/lowl_audio_driver.h"

namespace Lowl::Audio {

    class WasapiDriver : public AudioDriver {

    private:
        void create_devices(Error &error);

    public:
        void initialize(Error &error) override;

        WasapiDriver();

        ~WasapiDriver();
    };
}

#endif /* LOWL_DRIVER_WASAPI */
#endif /* LOWL_AUDIO_WASAPI_DRIVER_H */
