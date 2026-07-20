#ifndef LOWL_AUDIO_BUS_EVENT_H
#define LOWL_AUDIO_BUS_EVENT_H

#include "audio/source/lowl_audio_bus_slot_handle.h"
#include "audio/source/lowl_audio_source.h"

namespace Lowl::Audio {
    struct AudioBusEvent {
        enum class Type : uint8_t {
            Submit = 0,
            Remove = 1,
        };

        Type type = Type::Submit;
        AudioBusSlotHandle handle{};
        AudioSource *audio_source = nullptr;
        bool acknowledge_removal = false;
    };

    struct AudioBusAck {
        enum class Type : uint8_t {
            Removed = 0,
            Finished = 1,
            Rejected = 2,
        };

        Type type = Type::Removed;
        AudioBusSlotHandle handle{};
    };
} // namespace Lowl::Audio

#endif // LOWL_AUDIO_BUS_EVENT_H
