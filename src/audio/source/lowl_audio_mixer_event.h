#ifndef LOWL_AUDIO_MIXER_EVENT_H
#define LOWL_AUDIO_MIXER_EVENT_H

#include "audio/source/lowl_audio_mixer_handle.h"

namespace Lowl::Audio {
    /**
     * Queued command used to connect a source to the mixer.
     */
    struct AudioMixerEvent {
        uint64_l generation = 0;
        AudioPlaybackId connection_id = 0;
    };

    struct AudioMixerCompletion {
        enum class Type : uint8_t {
            Removed = 0,
            Finished = 1,
        };

        Type type = Type::Removed;
        AudioMixerHandle handle{};
    };
} // namespace Lowl::Audio

#endif // LOWL_AUDIO_MIXER_EVENT_H
