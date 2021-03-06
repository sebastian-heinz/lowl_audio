#ifndef LOWL_AUDIO_UTIL_H
#define LOWL_AUDIO_UTIL_H

#include "lowl_audio_stream.h"
#include "lowl_audio_data.h"
#include "lowl_release_pool.h"

#include <memory>

namespace Lowl {
    class AudioUtil {
    private:
        AudioUtil() {
            // Disallow creating an instance of this object
        };

    public:
        static std::unique_ptr<AudioStream> to_stream(std::shared_ptr<AudioData> p_audio_data);
        static std::unique_ptr<Lowl::ReleasePool> release_pool;
    };
}

#endif // LOWL_AUDIO_UTIL_H
