#ifndef LOWL_DRIVER_PA_H
#define LOWL_DRIVER_PA_H

#ifdef LOWL_DRIVER_PORTAUDIO

#include "lowl_driver.h"

#include <portaudio.h>

namespace Lowl {

    class PaDriver : public Driver {

    private:
        void create_devices(Error &error);

    public:
        void initialize(Error &error) override;

    public:
        PaDriver();

        ~PaDriver() override;
    };
}

#endif /* LOWL_DRIVER_PORTAUDIO */
#endif /* LOWL_PA_DRIVER_H */
