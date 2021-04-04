#include "lowl_audio_source.h"

Lowl::AudioSource::AudioSource(SampleRate p_sample_rate, Channel p_channel) {
    sample_rate = p_sample_rate;
    channel = p_channel;
}

Lowl::SampleRate Lowl::AudioSource::get_sample_rate() const {
    return sample_rate;
}

Lowl::Channel Lowl::AudioSource::get_channel() const {
    return channel;
}

Lowl::SampleFormat Lowl::AudioSource::get_sample_format() const {
    return Lowl::SampleFormat::FLOAT_32;
}

int Lowl::AudioSource::get_channel_num() const {
    return Lowl::get_channel_num(channel);
}
