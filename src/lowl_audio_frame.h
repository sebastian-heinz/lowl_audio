#ifndef LOWL_AUDIO_FRAME_H
#define LOWL_AUDIO_FRAME_H

#include "lowl_channel.h"

#include <vector>

namespace Lowl {
    struct AudioFrame {
        float left;
        float right;
    };
}

#endif //LOWL_AUDIO_FRAME_H
