#ifndef LOWL_AUDIO_UTIL_H
#define LOWL_AUDIO_UTIL_H

#include "audio/lowl_audio_stream.h"
#include "audio/lowl_audio_data.h"

#include <memory>

namespace Lowl::Audio {

    inline size_t ms_to_samples(size_t ms, SampleRate sample_rate, AudioChannel channel) {
        return (ms * (size_t) sample_rate * (size_t) get_channel_num(channel)) / 1000;
    }

    class Utilities {

    private:
        Utilities() {
            // Disallow creating an instance of this object
        };

    public:
        static std::unique_ptr<AudioStream> to_stream(std::shared_ptr<AudioData> p_audio_data);
    };
}

#endif // LOWL_AUDIO_UTIL_H
