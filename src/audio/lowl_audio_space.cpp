#include "lowl_audio_space.h"

#include "lowl_logger.h"

#include "audio/reader/lowl_audio_reader.h"

#include "audio/convert/lowl_audio_re_sampler.h"
#include "audio/convert/lowl_audio_channel_converter.h"

Lowl::Audio::AudioSpace::AudioSpace(SampleRate p_sample_rate, AudioChannel p_channel) : AudioSource(p_sample_rate,
                                                                                                    p_channel) {
    mixer = std::make_unique<AudioMixer>(sample_rate, channel);
    current_id = FirstSpaceId;
    audio_data_lookup = std::vector<std::shared_ptr<AudioData>>();
}

Lowl::Audio::AudioSpace::~AudioSpace() {
}

Lowl::SpaceId Lowl::Audio::AudioSpace::insert_audio_data(std::shared_ptr<AudioData> p_audio_data) {
    Lowl::SpaceId id = current_id;
    if (audio_data_lookup.size() < id + 1) {
        audio_data_lookup.resize(id + LookupGrowth);
    }
    audio_data_lookup[id] = p_audio_data;
    current_id++;
    return id;
}

Lowl::SpaceId Lowl::Audio::AudioSpace::add_audio(std::unique_ptr<AudioData> p_audio_data, Error &error) {

    std::shared_ptr<AudioData> audio = std::move(p_audio_data);
    SampleRate rate = audio->get_sample_rate();
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wfloat-equal"
    if (rate != sample_rate) {
#pragma clang diagnostic pop
       // std::unique_ptr<AudioData> resampled = ReSampler::resample(audio, sample_rate);
       // audio = std::move(resampled);
    }

    AudioChannel ch = audio->get_channel();
    if (ch != channel) {
        ChannelConverter channel_converter;
        std::unique_ptr<AudioData> converted = channel_converter.convert(channel, audio, error);
        if (error.has_error()) {
            LOWL_LOG_ERROR("Lowl::Space::load channel_converter.convert() ErrCode:" +
                           std::to_string(error.get_error_code()) +
                           " ErrText:" + error.get_error_text() + ". Could not convert channels from " +
                           std::to_string((int) ch) + " to " +
                           std::to_string((int) channel) + " channel.");
            return InvalidSpaceId;
        }
        audio = std::move(converted);
    }

    SpaceId audio_data_id = insert_audio_data(audio);

    return audio_data_id;
}

Lowl::SpaceId Lowl::Audio::AudioSpace::add_audio(const std::string &p_path, Error &error) {
    std::unique_ptr<AudioData> audio_data = AudioReader::create_data(p_path, error);
    if (error.has_error()) {
        return InvalidSpaceId;
    }
    return add_audio(std::move(audio_data), error);
}

void Lowl::Audio::AudioSpace::clear_all_audio() {
    stop_all_audio();
    audio_data_lookup.clear();
    current_id = FirstSpaceId;
    insert_audio_data(std::shared_ptr<AudioData>());
}

void Lowl::Audio::AudioSpace::stop_all_audio() {
    for (Lowl::SpaceId id = FirstSpaceId; id < current_id; id++) {
        stop(id);
    }
}

void Lowl::Audio::AudioSpace::play(SpaceId p_id, Volume p_volume, Panning p_panning) const {
    std::shared_ptr<AudioData> audio_data = get_audio_data(p_id);
    if (!audio_data) {
        return;
    }
    audio_data->set_volume(p_volume);
    audio_data->set_panning(p_panning);
    mixer->mix(audio_data);
}

void Lowl::Audio::AudioSpace::play(SpaceId p_id) const {
    std::shared_ptr<AudioData> audio_data = get_audio_data(p_id);
    if (!audio_data) {
        return;
    }
    mixer->mix(audio_data);
}

void Lowl::Audio::AudioSpace::stop(SpaceId p_id) const {
    std::shared_ptr<AudioData> audio_data = get_audio_data(p_id);
    if (!audio_data) {
        return;
    }
    mixer->remove(audio_data);
}

void Lowl::Audio::AudioSpace::set_volume(SpaceId p_id, Volume p_volume) const {
    std::shared_ptr<AudioData> audio_data = get_audio_data(p_id);
    if (!audio_data) {
        return;
    }
    audio_data->set_volume(p_volume);
}

void Lowl::Audio::AudioSpace::set_panning(SpaceId p_id, Panning p_panning) const {
    std::shared_ptr<AudioData> audio_data = get_audio_data(p_id);
    if (!audio_data) {
        return;
    }
    audio_data->set_panning(p_panning);
}

void Lowl::Audio::AudioSpace::seek_frame(SpaceId p_id, size_t p_frame) const {
    std::shared_ptr<AudioData> audio_data = get_audio_data(p_id);
    if (!audio_data) {
        return;
    }
    audio_data->seek_frame(p_frame);
}

void Lowl::Audio::AudioSpace::seek_time(SpaceId p_id, double_l p_seconds) const {
    std::shared_ptr<AudioData> audio_data = get_audio_data(p_id);
    if (!audio_data) {
        return;
    }
    audio_data->seek_time(p_seconds);
}

void Lowl::Audio::AudioSpace::reset(SpaceId p_id) const {
    std::shared_ptr<AudioData> audio_data = get_audio_data(p_id);
    if (!audio_data) {
        return;
    }
    audio_data->reset();
}

Lowl::size_l Lowl::Audio::AudioSpace::get_frame_position(SpaceId p_id) const {
    std::shared_ptr<AudioData> audio_data = get_audio_data(p_id);
    if (!audio_data) {
        return 0;
    }
    return audio_data->get_frame_position();
}

Lowl::size_l Lowl::Audio::AudioSpace::get_frames_remaining(SpaceId p_id) const {
    std::shared_ptr<AudioData> audio_data = get_audio_data(p_id);
    if (!audio_data) {
        return 0;
    }
    return audio_data->get_frames_remaining();
}

Lowl::size_l Lowl::Audio::AudioSpace::get_frame_count(SpaceId p_id) const {
    std::shared_ptr<AudioData> audio_data = get_audio_data(p_id);
    if (!audio_data) {
        return 0;
    }
    return audio_data->get_frame_count();
}

std::shared_ptr<Lowl::Audio::AudioData> Lowl::Audio::AudioSpace::get_audio_data(SpaceId p_id) const {
    if (p_id >= current_id) {
        return nullptr;
    }
    std::shared_ptr<AudioData> audio_data = audio_data_lookup[p_id];
    return audio_data;
}

Lowl::Audio::AudioSource::ReadResult Lowl::Audio::AudioSpace::read(AudioFrame &audio_frame) {
    ReadResult result = mixer->read(audio_frame);
    if (result == ReadResult::Read) {
        process_volume(audio_frame);
        process_panning(audio_frame);
    }
    return result;
}

Lowl::size_l Lowl::Audio::AudioSpace::get_frames_remaining() const {
    return 1;
}

Lowl::size_l Lowl::Audio::AudioSpace::get_frame_position() const {
    return 0;
}

Lowl::size_l Lowl::Audio::AudioSpace::get_frame_count() const {
    return 0;
}

std::map<Lowl::SpaceId, std::string> Lowl::Audio::AudioSpace::get_name_mapping() const {
    std::map<SpaceId, std::string> map = std::map<SpaceId, std::string>();
    for (SpaceId space_id = 0; space_id < audio_data_lookup.size(); space_id++) {
        std::shared_ptr<AudioData> audio_data = audio_data_lookup[space_id];
        if (!audio_data) {
            continue;
        }
        map.insert_or_assign(space_id, audio_data->get_name());
    }
    return map;
}
