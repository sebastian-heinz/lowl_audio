#ifndef LOWL_RE_SAMPLER_R8B_H
#define LOWL_RE_SAMPLER_R8B_H

#include "audio/source/lowl_audio_data.h"

namespace Lowl::Audio {
    class ReSamplerR8b {
    public:
        static std::unique_ptr<Lowl::Audio::AudioData> resample(std::shared_ptr<AudioData> p_audio_data,
                                                                SampleRate p_sample_rate_dst);
    };
} // namespace Lowl::Audio

#endif
