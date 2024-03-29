#ifndef LOWL_AUDIO_DUMMY_DRIVER_H
#define LOWL_AUDIO_DUMMY_DRIVER_H

#ifdef LOWL_DRIVER_DUMMY

#include "audio/lowl_audio_driver.h"

namespace Lowl::Audio {

    class AudioDriverDummy : public Lowl::Audio::AudioDriver {

    public:
        void initialize(Error &error) override;

        AudioDriverDummy();

        ~AudioDriverDummy() override = default;
    };
}

#endif /* LOWL_DRIVER_DUMMY */
#endif /* LOWL_AUDIO_DUMMY_DRIVER_H */
