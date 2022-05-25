#ifndef LOWL_AUDIO_WASAPI_COM_H
#define LOWL_AUDIO_WASAPI_COM_H

#ifdef LOWL_DRIVER_WASAPI

#include "lowl_error.h"

#include <memory>

namespace Lowl::Audio {

    class WasapiCom {
    private:
        bool initialized;
        bool already_initialized;

    public:
        static std::unique_ptr<Audio::WasapiCom> wasapi_com;

        WasapiCom();

        ~WasapiCom();

        void initialize(Error &error);

        void terminate();
    };

} //namespace Lowl::Audio

#endif /* LOWL_DRIVER_WASAPI */
#endif /* LOWL_AUDIO_WASAPI_COM_H */
