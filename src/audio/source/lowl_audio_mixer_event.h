#ifndef LOWL_AUDIO_MIXER_EVENT_H
#define LOWL_AUDIO_MIXER_EVENT_H

#include "audio/source/lowl_audio_mixer_handle.h"

namespace Lowl::Audio {
    /**
     * Queued command used to connect a source to the mixer.
     */
    struct AudioMixerEvent {
        AudioGeneration generation = 0;
        AudioMixerConnectionId connection_id = 0;
    };

    struct AudioMixerCompletion {
        enum class Type : uint8_t {
            Removed = 0,
            Finished = 1,
        };

        Type type = Type::Removed;
        AudioMixerHandle handle{};
    };

    static_assert(sizeof(AudioMixerEvent) == 8,
                  "AudioMixerEvent must remain compact for the fixed command queue");
    static_assert(sizeof(AudioMixerCompletion) == 16,
                  "AudioMixerCompletion must remain compact for the fixed completion queue");
} // namespace Lowl::Audio

#endif // LOWL_AUDIO_MIXER_EVENT_H
