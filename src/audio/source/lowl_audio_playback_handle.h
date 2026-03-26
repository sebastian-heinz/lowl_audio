#ifndef LOWL_AUDIO_PLAYBACK_HANDLE_H
#define LOWL_AUDIO_PLAYBACK_HANDLE_H

#include "lowl_typedef.h"

namespace Lowl {
    struct AudioPlaybackHandle {
        AudioPlaybackId id = 0;
        uint16_l generation = 0;

        bool is_valid() const {
            return id != 0 && generation != 0;
        }

        bool operator==(const AudioPlaybackHandle &p_other) const {
            return id == p_other.id && generation == p_other.generation;
        }

        bool operator!=(const AudioPlaybackHandle &p_other) const {
            return !(*this == p_other);
        }
    };
} // namespace Lowl

#endif // LOWL_AUDIO_PLAYBACK_HANDLE_H
