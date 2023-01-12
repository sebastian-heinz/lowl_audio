#include "lowl_audio_re_sampler.h"

Lowl::Audio::ReSampler::ReSampler(Lowl::SampleRate p_sample_rate_src, Lowl::SampleRate p_sample_rate_dst,
                                  Lowl::Audio::AudioChannel p_channel) {
    sample_rate_src = p_sample_rate_src;
    sample_rate_dst = p_sample_rate_dst;
    channel = p_channel;
}

void Lowl::Audio::ReSampler::flush() {
}

