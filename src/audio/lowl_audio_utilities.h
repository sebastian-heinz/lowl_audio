#ifndef LOWL_AUDIO_UTIL_H
#define LOWL_AUDIO_UTIL_H

#include <memory>

#include "audio/lowl_audio_format.h"
#include "lowl_error.h"

namespace Lowl::Audio {
    class AudioStream;
    class AudioData;

    LOWL_INLINE size_t ms_to_samples(const size_t ms, const SampleRate sample_rate, const uint8_t channel_count) {
        return ms * static_cast<size_t>(sample_rate) * channel_count / 1000;
    }

    class Utilities {
    private:
        Utilities() {
            // Disallow creating an instance of this object
        };

    public:
        static std::unique_ptr<AudioData> clone_audio_data(const std::shared_ptr<AudioData> &p_audio_data);
        static std::unique_ptr<AudioStream> to_stream(const std::shared_ptr<AudioData> &p_audio_data, Error &error);
    };
} // namespace Lowl::Audio

#endif // LOWL_AUDIO_UTIL_H
