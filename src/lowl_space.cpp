#include "lowl_space.h"

#include "lowl_audio_reader.h"
#include "lowl_re_sampler.h"
#include "lowl_channel_converter.h"

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

void Lowl::Space::play(SpaceId p_id, Volume p_volume, Panning p_panning) {
    std::shared_ptr<AudioData> audio_data = get_audio_data(p_id);
    if (!audio_data) {
        return;
    }
    audio_data->set_volume(p_volume);
    audio_data->set_panning(p_panning);
    audio_data->reset_read();
    mixer->mix_data(audio_data);
}

void Lowl::Space::stop(SpaceId p_id) {
    std::shared_ptr<AudioData> audio_data = get_audio_data(p_id);
    if (!audio_data) {
        return;
    }
    audio_data->cancel_read();
}

void Lowl::Space::load() {

    if (channel == Channel::None) {
        std::map<Channel, int> channels = std::map<Channel, int>();
        for (SpaceId i = 1; i < audio_data_lookup.size(); i++) {
            std::shared_ptr<AudioData> audio = audio_data_lookup[i];
            Channel ch = audio->get_channel();
            std::map<Channel, int>::iterator it = channels.find(ch);
            if (it == channels.end()) {
                channels.insert(std::make_pair(ch, 1));
            } else {
                it->second++;
            }
        }
        std::map<Channel, int>::iterator most_frequent = std::max_element(
                channels.begin(),
                channels.end(),
                [](const std::pair<Channel, int> &a, const std::pair<Channel, int> &b) -> bool {
                    return a.second < b.second;
                }
        );
        channel = most_frequent->first;
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

    ChannelConverter channel_converter;
    for (SpaceId i = 1; i < audio_data_lookup.size(); i++) {
        std::shared_ptr<AudioData> audio = audio_data_lookup[i];
        SampleRate rate = audio->get_sample_rate();
        if (rate != sample_rate) {
            std::unique_ptr<AudioData> resampled = ReSampler::resample(audio, sample_rate);
            audio_data_lookup[i] = std::move(resampled);
            audio = audio_data_lookup[i];
        }
        Channel ch = audio->get_channel();
        if (ch != channel) {
            Error error;
            std::unique_ptr<AudioData> converted = channel_converter.convert(ch, audio, error);
            if (error.has_error()) {
                // TODO warning / error ?
                audio_data_lookup[i] = nullptr;
                continue;
            }
            audio_data_lookup[i] = std::move(converted);
            audio = audio_data_lookup[i];
        }
    }

    mixer = std::make_shared<AudioMixer>(sample_rate, channel);
}

void Lowl::Space::set_sample_rate(SampleRate p_sample_rate) {
    sample_rate = p_sample_rate;
}

void Lowl::Space::set_channel(Channel p_channel) {
    channel = p_channel;
}

void Lowl::Space::set_volume(SpaceId p_id, Volume p_volume) {
    std::shared_ptr<AudioData> audio_data = get_audio_data(p_id);
    if (!audio_data) {
        return;
    }
    audio_data->set_volume(p_volume);
}

void Lowl::Space::set_panning(SpaceId p_id, Panning p_panning) {
    std::shared_ptr<AudioData> audio_data = get_audio_data(p_id);
    if (!audio_data) {
        return;
    }
    audio_data->set_panning(p_panning);
}

std::shared_ptr<Lowl::AudioData> Lowl::Space::get_audio_data(SpaceId p_id) {
    if (p_id >= current_id) {
        return nullptr;
    }
    std::shared_ptr<AudioData> audio_data = audio_data_lookup[p_id];
    return audio_data;
}

std::shared_ptr<Lowl::AudioMixer> Lowl::Space::get_mixer() {
    return mixer;
}

Lowl::Space::~Space() {
}
