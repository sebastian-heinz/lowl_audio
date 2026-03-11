#ifndef LOWL_AUDIO_RE_SAMPLER_SOURCE_H
#define LOWL_AUDIO_RE_SAMPLER_SOURCE_H

#include "audio/source/lowl_audio_source.h"
#include "audio/convert/lowl_audio_re_sampler.h"

#include <memory>

namespace Lowl::Audio {
    class ReSamplerSource : AudioSource {
    private:
        std::shared_ptr<AudioSource> audio_source;
        std::unique_ptr<ReSampler> re_sampler;
        SampleRate sample_rate_dst;
        uint32_t max_frames;

    public:
        size_l get_frames_remaining() const override;

        size_l get_frame_position() const override;

        size_l get_frame_count() const override;

        ReadResult read(AudioFrame &audio_frame) override;

        ReSamplerSource(std::unique_ptr<ReSampler> p_re_sampler,
                        std::shared_ptr<AudioSource> p_audio_source,
                        SampleRate p_sample_rate_dst
        );

        ~ReSamplerSource() override = default;
    };
}


#endif //LOWL_AUDIO_SOURCE_H
