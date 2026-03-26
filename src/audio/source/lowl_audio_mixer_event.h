#ifndef LOWL_AUDIO_MIXER_EVENT_H
#define LOWL_AUDIO_MIXER_EVENT_H

#include "audio/source/lowl_audio_source.h"

namespace Lowl::Audio {
    /**
     * Queued command used to add or remove a source from the mixer thread-safely.
     */
    struct AudioMixerEvent {
        enum class Type : uint8_t {
            Mix = 0,
            Remove = 1,
        };

        Type type = Type::Mix;
        AudioMixerHandle handle{};
        AudioSource *audio_source = nullptr;
        bool acknowledge_removal = false;
    };

    struct AudioMixerAck {
        enum class Type : uint8_t {
            Removed = 0,
            Finished = 1,
            Rejected = 2,
        };

        Type type = Type::Removed;
        AudioMixerHandle handle{};
    };
} // namespace Lowl::Audio

#endif // LOWL_AUDIO_MIXER_EVENT_H
