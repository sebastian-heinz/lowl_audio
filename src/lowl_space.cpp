#include "lowl_space.h"

#include "lowl_audio_reader.h"
#include "lowl_re_sampler.h"

#include <map>

Lowl::Space::Space() {
    current_id = 1;
    audio_data_lookup = std::vector<std::shared_ptr<AudioData>>();
    audio_data_lookup.push_back(std::shared_ptr<AudioData>());
    is_loaded = false;
    sample_rate = 0;
    channel = Channel::None;
    mixer = std::unique_ptr<AudioMixer>();
}

Lowl::SpaceId Lowl::Space::add_audio(std::unique_ptr<AudioData> p_audio_data, Error &error) {
    if (is_loaded) {
        error.set_error(ErrorCode::Error);
        return InvalidSpaceId;
    }
    audio_data_lookup.push_back(std::move(p_audio_data));
    SpaceId audio_data_id = current_id;
    current_id++;
    return audio_data_id;
}

Lowl::SpaceId Lowl::Space::add_audio(const std::string &p_path, Error &error) {
    if (is_loaded) {
        return InvalidSpaceId;
    }
    std::unique_ptr<AudioData> audio_data = AudioReader::create_data(p_path, error);
    if (error.has_error()) {
        error.set_error(ErrorCode::Error);
        return InvalidSpaceId;
    }
    return add_audio(std::move(audio_data), error);
}

void Lowl::Space::play(Lowl::SpaceId p_id) {
    std::shared_ptr<AudioData> audio_data = audio_data_lookup[p_id];
    if (!audio_data) {
        return;
    }
    mixer->mix_data(audio_data);
}

void Lowl::Space::stop(Lowl::SpaceId p_id) {
    std::shared_ptr<AudioData> audio_data = audio_data_lookup[p_id];
    if (!audio_data) {
        return;
    }
    audio_data->cancel_read();
}

void Lowl::Space::load() {

    if (channel == Channel::None) {
        // todo detect channel
        for (SpaceId i = 1; i < audio_data_lookup.size(); i++) {
            std::shared_ptr<AudioData> audio = audio_data_lookup[i];
            Channel ch = audio->get_channel();
            // todo
            channel = ch;
        }
    }

    if (sample_rate == 0) {
        std::map<SampleRate, int> sample_rates = std::map<SampleRate, int>();
        for (SpaceId i = 1; i < audio_data_lookup.size(); i++) {
            std::shared_ptr<AudioData> audio = audio_data_lookup[i];
            SampleRate rate = audio->get_sample_rate();
            std::map<SampleRate, int>::iterator it = sample_rates.find(rate);
            if (it == sample_rates.end()) {
                sample_rates.insert(std::make_pair(rate, 1));
            } else {
                it->second++;
            }
        }
        std::map<SampleRate, int>::iterator most_frequent = std::max_element(
                sample_rates.begin(),
                sample_rates.end(),
                [](const std::pair<SampleRate, int> &a, const std::pair<SampleRate, int> &b) -> bool {
                    return a.second < b.second;
                }
        );
        sample_rate = most_frequent->first;
    }

    for (SpaceId i = 1; i < audio_data_lookup.size(); i++) {
        std::shared_ptr<AudioData> audio = audio_data_lookup[i];
        SampleRate rate = audio->get_sample_rate();
        if (rate != sample_rate) {
            std::unique_ptr<AudioData> resampled = ReSampler::resample(audio, sample_rate, 512);
            audio_data_lookup[i] = std::move(resampled);
            audio = audio_data_lookup[i];
        }
        Channel ch = audio->get_channel();
        if (ch != channel) {
            // todo ...
        }
    }

    mixer = std::make_unique<AudioMixer>(sample_rate, channel);
    mixer->start_mix();

    is_loaded = true;
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