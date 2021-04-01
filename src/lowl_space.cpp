#include "lowl_space.h"

Lowl::Space::Space() {
    current_id = 1;
    audio_data_lookup = std::vector<std::shared_ptr<AudioData>>();
    audio_data_lookup.push_back(std::shared_ptr<AudioData>());
    is_loaded = false;
    sample_rate = 0;
    channel = Channel::None;
    mixer = std::unique_ptr<AudioMixer>();
}

Lowl::SpaceId Lowl::Space::add_audio(std::unique_ptr<AudioData> p_audio_data) {
    if (is_loaded) {
        return InvalidSpaceId;
    }
    audio_data_lookup.push_back(std::move(p_audio_data));
    SpaceId audio_data_id = current_id;
    current_id++;
    return audio_data_id;
}

Lowl::SpaceId Lowl::Space::add_audio(const std::string &p_path) {
    if (is_loaded) {
        return InvalidSpaceId;
    }
    // TODO
    return InvalidSpaceId;
}

void Lowl::Space::play(Lowl::SpaceId p_id) {
    std::shared_ptr<AudioData> audio_data = audio_data_lookup[p_id];
    mixer->mix_data(audio_data);
}

void Lowl::Space::stop(Lowl::SpaceId p_id) {
    std::shared_ptr<AudioData> audio_data = audio_data_lookup[p_id];
    audio_data->cancel_read();
}

void Lowl::Space::load() {
    is_loaded = true;

    if (channel == Channel::None) {
        // todo detect channel
    }

    if (sample_rate == 0) {
        // todo detect sample rate
    }

    for (std::shared_ptr<AudioData> audio : audio_data_lookup) {
        SampleRate rate = audio->get_sample_rate();
        Channel ch = audio->get_channel();

    }

    mixer = std::make_unique<AudioMixer>(sample_rate, channel);
    // TODO find most common sample rate & resample all audio data to it, if any diff
}

void Lowl::Space::set_sample_rate(Lowl::SampleRate p_sample_rate) {
    sample_rate = p_sample_rate;
}

void Lowl::Space::set_channel(Lowl::Channel p_channel) {
    channel = p_channel;
}

std::shared_ptr<Lowl::AudioStream> Lowl::Space::get_out_stream() {
    return mixer->get_out_stream();
}

