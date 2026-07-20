#ifndef LOWL_AUDIO_BUS_SLOT_HANDLE_H
#define LOWL_AUDIO_BUS_SLOT_HANDLE_H

#include "lowl_typedef.h"

namespace Lowl {
    struct AudioBusSlotHandle {
        AudioPlaybackId slot_id = 0;
        uint16_l generation = 0;

        bool is_valid() const {
            return slot_id != 0 && generation != 0;
        }

        bool operator==(const AudioBusSlotHandle &p_other) const {
            return slot_id == p_other.slot_id && generation == p_other.generation;
        }

        bool operator!=(const AudioBusSlotHandle &p_other) const {
            return !(*this == p_other);
        }
    };
} // namespace Lowl

#endif // LOWL_AUDIO_BUS_SLOT_HANDLE_H
