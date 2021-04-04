#ifndef LOWL_AUDIO_UTIL_H
#define LOWL_AUDIO_UTIL_H

#include "lowl_audio_stream.h"
#include "lowl_audio_data.h"

#include <memory>

namespace Lowl {
    class AudioUtil {
    public:
        static std::unique_ptr<AudioStream> to_stream(std::shared_ptr<AudioData> p_audio_data);

    };
}

#endif // LOWL_AUDIO_UTIL_H
