#ifndef LOWL_AUDIO_LOWL_AUDIO_BLOCK_H
#define LOWL_AUDIO_LOWL_AUDIO_BLOCK_H

#include <array>

#include "lowl_typedef.h"

namespace Lowl::Audio {
    struct AudioBlock {
        static constexpr unsigned int MAX_CHANNEL = 8;
        std::array<Sample *, MAX_CHANNEL> channels{};
        size_t frame_count = 0;
        uint8_t active_channels = 2;
    };
}

#endif //LOWL_AUDIO_LOWL_AUDIO_BLOCK_H
