#ifndef LOWL_AUDIO_MIXER_EVENT_H
#define LOWL_AUDIO_MIXER_EVENT_H

#include "audio/source/lowl_audio_source.h"

namespace Lowl::Audio {
    struct AudioMixerEvent {
        enum class Type : uint8_t {
            Mix = 0,
            Remove = 1,
        };

        Type type = Type::Mix;
        AudioSource *audio_source = nullptr;
    };
}

#endif //LOWL_AUDIO_MIXER_EVENT_H
