#include "lowl_audio_re_sampler_source.h"

#include <cmath>
#include <algorithm>

Lowl::Audio::AudioSource::ReadResult
Lowl::Audio::ReSamplerSource::read(Lowl::Audio::AudioFrame &audio_frame) {

    AudioFrame frame{};
    while (audio_source->read(frame) == AudioSource::ReadResult::Read) {
        re_sampler->write(frame);
    }
    if(re_sampler->read(audio_frame)){
        return AudioSource::ReadResult::Read;
    }

    return Lowl::Audio::AudioSource::ReadResult::End;
}

Lowl::Audio::ReSamplerSource::ReSamplerSource(std::unique_ptr<Lowl::Audio::ReSampler> p_re_sampler,
                                              std::shared_ptr<Lowl::Audio::AudioSource> p_audio_source,
                                              Lowl::SampleRate p_sample_rate_dst)
        : AudioSource(p_sample_rate_dst, p_audio_source->get_channel()) {
    re_sampler = std::move(p_re_sampler);
    audio_source = p_audio_source;
    sample_rate_dst = p_sample_rate_dst;
    max_frames = 100;
}

Lowl::size_l Lowl::Audio::ReSamplerSource::get_frames_remaining() const {
    return 0;
}

Lowl::size_l Lowl::Audio::ReSamplerSource::get_frame_position() const {
    return 0;
}

Lowl::size_l Lowl::Audio::ReSamplerSource::get_frame_count() const {
    return 0;
}
