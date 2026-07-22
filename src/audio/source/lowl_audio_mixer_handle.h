#ifndef LOWL_AUDIO_MIXER_HANDLE_H
#define LOWL_AUDIO_MIXER_HANDLE_H

#include "lowl_typedef.h"

namespace Lowl {
    struct AudioMixerHandle {
        uint64_l mixer_id = 0;
        AudioPlaybackId connection_id = 0;
        uint64_l generation = 0;

        bool is_valid() const {
            return mixer_id != 0 && connection_id != 0 && generation != 0;
        }

        bool operator==(const AudioMixerHandle &p_other) const {
            return mixer_id == p_other.mixer_id && connection_id == p_other.connection_id &&
                   generation == p_other.generation;
        }

        bool operator!=(const AudioMixerHandle &p_other) const {
            return !(*this == p_other);
        }
    };
} // namespace Lowl

#endif // LOWL_AUDIO_MIXER_HANDLE_H
