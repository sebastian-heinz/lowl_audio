#ifndef LOWL_AUDIO_MIXER_EVENT_H
#define LOWL_AUDIO_MIXER_EVENT_H

#include "audio/source/lowl_audio_mixer_handle.h"
#include "audio/source/lowl_audio_source.h"

namespace Lowl::Audio {
    /**
     * Queued command used to connect or disconnect a source from the mixer.
     */
    struct AudioMixerEvent {
        enum class Type : uint8_t {
            Connect = 0,
            Disconnect = 1,
        };

        Type type = Type::Connect;
        AudioMixerHandle handle{};
        AudioSource *audio_source = nullptr;
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
