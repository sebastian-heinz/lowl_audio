#ifndef LOWL_PA_DRIVER_H
#define LOWL_PA_DRIVER_H

#ifdef LOWL_DRIVER_PORTAUDIO

#include <portaudio.h>

#include "../../lowl_driver.h"
#include "../../lowl_error.h"

namespace Lowl {

    class PaDriver : public Driver {

    private:
        void create_devices(LowlError &error);

    public:
        void initialize(LowlError &error) override;

    public:
        PaDriver();

        ~PaDriver() override;
    };
}

#endif /* LOWL_DRIVER_PORTAUDIO */
#endif /* LOWL_PA_DRIVER_H */
