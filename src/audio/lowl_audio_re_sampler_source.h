#ifndef LOWL_AUDIO_RE_SAMPLER_SOURCE_H
#define LOWL_AUDIO_RE_SAMPLER_SOURCE_H

#include "audio/lowl_audio_source.h"
#include "audio/convert/lowl_audio_re_sampler.h"


namespace Lowl::Audio {

    class ReSamplerSource : AudioSource {

    private:
        std::shared_ptr<Lowl::Audio::AudioSource> audio_source;
        std::unique_ptr<Lowl::Audio::ReSampler> re_sampler;
        SampleRate sample_rate_dst;
        uint32_t max_frames;

    public:
        virtual size_l get_frames_remaining() const override;

        virtual size_l get_frame_position() const override;

        virtual size_l get_frame_count() const override;

        virtual ReadResult read(AudioFrame &audio_frame) override;

        ReSamplerSource(std::unique_ptr<Lowl::Audio::ReSampler> p_re_sampler,
                        std::shared_ptr<Lowl::Audio::AudioSource> p_audio_source,
                        Lowl::SampleRate p_sample_rate_dst
        );

        virtual ~ReSamplerSource() = default;
    };
}


#endif //LOWL_AUDIO_SOURCE_H
