#ifndef LOWL_AUDIO_UTIL_H
#define LOWL_AUDIO_UTIL_H

#include "lowl_typedef.h"
#include "lowl_error.h"

#include "audio/lowl_audio_channel.h"

#include <cmath>
#include <memory>

namespace Lowl::Audio {
    class AudioStream;
    class AudioData;

    _INLINE_ size_t ms_to_samples(
        const size_t ms,
        const SampleRate sample_rate,
        const AudioChannel channel
    ) {
        return ms * static_cast<size_t>(sample_rate) * get_channel_num(channel) / 1000;
    }

    _INLINE_ uint32_l normalize_sample_rate(const SampleRate p_rate) {
        return static_cast<uint32_l>(std::llround(p_rate));
    }

    _INLINE_ bool sample_rates_equal(const SampleRate p_a, const SampleRate p_b) {
        return normalize_sample_rate(p_a) == normalize_sample_rate(p_b);
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
