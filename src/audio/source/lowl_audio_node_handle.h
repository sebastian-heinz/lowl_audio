#ifndef LOWL_AUDIO_NODE_HANDLE_H
#define LOWL_AUDIO_NODE_HANDLE_H

#include "lowl_typedef.h"

namespace Lowl {
    struct AudioNodeHandle {
        uint64_l graph_id = 0;
        uint64_l node_id = 0;

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
} // namespace Lowl

#endif // LOWL_AUDIO_NODE_HANDLE_H
