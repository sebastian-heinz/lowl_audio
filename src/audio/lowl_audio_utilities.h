#ifndef LOWL_AUDIO_UTIL_H
#define LOWL_AUDIO_UTIL_H

#include "lowl_error.h"
#include "audio/source/lowl_audio_stream.h"
#include "audio/source/lowl_audio_data.h"


namespace Lowl::Audio {
    _INLINE_ size_t ms_to_samples(
        const size_t ms,
        const SampleRate sample_rate,
        const AudioChannel channel
    ) {
        return ms * static_cast<size_t>(sample_rate) * get_channel_num(channel) / 1000;
    }

    class Utilities {
    private:
        Utilities() {
            // Disallow creating an instance of this object
        };

    public:
        static std::unique_ptr<AudioStream> to_stream(const std::shared_ptr<AudioData> &p_audio_data, Error &error);
    };
}

#endif // LOWL_AUDIO_UTIL_H
