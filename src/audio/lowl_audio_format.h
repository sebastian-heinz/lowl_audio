#ifndef LOWL_AUDIO_FORMAT_H
#define LOWL_AUDIO_FORMAT_H

#include <cmath>

#include "audio/lowl_audio_channel.h"
#include "lowl_typedef.h"

namespace Lowl::Audio {
    LOWL_INLINE uint32_l normalize_sample_rate(const SampleRate p_rate) {
        return static_cast<uint32_l>(std::llround(p_rate));
    }

    LOWL_INLINE bool sample_rates_equal(const SampleRate p_a, const SampleRate p_b) {
        return std::isfinite(p_a) && std::isfinite(p_b) &&
               normalize_sample_rate(p_a) == normalize_sample_rate(p_b);
    }

    /**
     * Format shared by nodes inside the audio graph.
     *
     * Encoded file/container formats are represented by FileFormat. Scalar
     * representations at import and device boundaries are represented by
     * SampleFormat. Neither belongs in the graph compatibility type.
     */
    struct AudioFormat {
        SampleRate sample_rate = NO_SAMPLE_RATE;
        ChannelLayout channel_layout{};

        bool is_valid() const {
            return std::isfinite(sample_rate) && sample_rate > NO_SAMPLE_RATE && channel_layout.is_valid();
        }

        bool operator==(const AudioFormat &p_other) const {
            return sample_rates_equal(sample_rate, p_other.sample_rate) && channel_layout == p_other.channel_layout;
        }

        bool operator!=(const AudioFormat &p_other) const {
            return !(*this == p_other);
        }
    };
} // namespace Lowl::Audio

#endif
