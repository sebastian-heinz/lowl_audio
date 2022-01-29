#ifndef LOWL_AUDIO_MIXER_EVENT_H
#define LOWL_AUDIO_MIXER_EVENT_H

#include "audio/lowl_audio_source.h"

#include <memory>

namespace Lowl::Audio {
    struct AudioMixerEvent {
        static const uint8_t Mix = 0;
        static const uint8_t Remove = 1;

        uint8_t type;
        std::shared_ptr<AudioSource> audio_source;
    };
}

#endif //LOWL_AUDIO_MIXER_EVENT_H
