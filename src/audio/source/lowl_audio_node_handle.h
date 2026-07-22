#ifndef LOWL_AUDIO_NODE_HANDLE_H
#define LOWL_AUDIO_NODE_HANDLE_H

#include "lowl_typedef.h"

namespace Lowl {
    struct AudioNodeHandle {
        AudioInstanceId graph_id = 0;
        AudioNodeId node_id = 0;

        bool is_valid() const {
            return graph_id != 0 && node_id != 0;
        }

        bool operator==(const AudioNodeHandle &p_other) const {
            return graph_id == p_other.graph_id && node_id == p_other.node_id;
        }

        bool operator!=(const AudioNodeHandle &p_other) const {
            return !(*this == p_other);
        }
    };

    static_assert(sizeof(AudioNodeHandle) == 8,
                  "AudioNodeHandle must remain a compact pair of 32-bit identities");
} // namespace Lowl

#endif // LOWL_AUDIO_NODE_HANDLE_H
