#include "lowl_audio_space.h"

#include "lowl_audio_reader.h"
#include "lowl_re_sampler.h"
#include "lowl_audio_channel_converter.h"
#include "lowl_logger.h"

#include <map>

Lowl::AudioSpace::AudioSpace(Lowl::SampleRate p_sample_rate, Lowl::AudioChannel p_channel) : AudioSource(p_sample_rate, p_channel) {
    current_id = 1;
    audio_data_lookup = std::vector<std::shared_ptr<AudioData>>();
    audio_data_lookup.push_back(std::shared_ptr<AudioData>());
    mixer = std::make_unique<AudioMixer>(sample_rate, channel);
}

Lowl::AudioSpace::~AudioSpace() {
}

Lowl::SpaceId Lowl::AudioSpace::add_audio(std::unique_ptr<AudioData> p_audio_data, Error &error) {

    std::shared_ptr<AudioData> audio = std::move(p_audio_data);
    SampleRate rate = audio->get_sample_rate();
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wfloat-equal"
    if (rate != sample_rate) {
#pragma clang diagnostic pop
        std::unique_ptr<AudioData> resampled = ReSampler::resample(audio, sample_rate);
        audio = std::move(resampled);
    }

    AudioChannel ch = audio->get_channel();
    if (ch != channel) {
        AudioChannelConverter channel_converter;
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

    audio_data_lookup.push_back(audio);
    SpaceId audio_data_id = current_id;
    current_id++;
    return audio_data_id;
}

Lowl::SpaceId Lowl::AudioSpace::add_audio(const std::string &p_path, Error &error) {
    std::unique_ptr<AudioData> audio_data = AudioReader::create_data(p_path, error);
    if (error.has_error()) {
        return InvalidSpaceId;
    }
    return add_audio(std::move(audio_data), error);
}

void Lowl::AudioSpace::play(SpaceId p_id, Volume p_volume, Panning p_panning) const {
    std::shared_ptr<AudioData> audio_data = get_audio_data(p_id);
    if (!audio_data) {
        return;
    }
    audio_data->set_volume(p_volume);
    audio_data->set_panning(p_panning);
    mixer->mix(audio_data);
}

void Lowl::AudioSpace::play(Lowl::SpaceId p_id) const {
    std::shared_ptr<AudioData> audio_data = get_audio_data(p_id);
    if (!audio_data) {
        return;
    }
    mixer->mix(audio_data);
}

void Lowl::AudioSpace::stop(SpaceId p_id) const {
    std::shared_ptr<AudioData> audio_data = get_audio_data(p_id);
    if (!audio_data) {
        return;
    }
    mixer->remove(audio_data);
}

void Lowl::AudioSpace::set_volume(SpaceId p_id, Volume p_volume) const {
    std::shared_ptr<AudioData> audio_data = get_audio_data(p_id);
    if (!audio_data) {
        return;
    }
    audio_data->set_volume(p_volume);
}

void Lowl::AudioSpace::set_panning(SpaceId p_id, Panning p_panning) const {
    std::shared_ptr<AudioData> audio_data = get_audio_data(p_id);
    if (!audio_data) {
        return;
    }
    audio_data->set_panning(p_panning);
}

void Lowl::AudioSpace::seek_frame(Lowl::SpaceId p_id, size_t p_frame) const {
    std::shared_ptr<AudioData> audio_data = get_audio_data(p_id);
    if (!audio_data) {
        return;
    }
    audio_data->seek_frame(p_frame);
}

void Lowl::AudioSpace::seek_time(Lowl::SpaceId p_id, Lowl::double_l p_seconds) const {
    std::shared_ptr<AudioData> audio_data = get_audio_data(p_id);
    if (!audio_data) {
        return;
    }
    audio_data->seek_time(p_seconds);
}

void Lowl::AudioSpace::reset(Lowl::SpaceId p_id) const {
    std::shared_ptr<AudioData> audio_data = get_audio_data(p_id);
    if (!audio_data) {
        return;
    }
    audio_data->reset();
}

Lowl::size_l Lowl::AudioSpace::get_frame_position(Lowl::SpaceId p_id) const {
    std::shared_ptr<AudioData> audio_data = get_audio_data(p_id);
    if (!audio_data) {
        return 0;
    }
    return audio_data->get_frame_position();
}

Lowl::size_l Lowl::AudioSpace::get_frames_remaining(Lowl::SpaceId p_id) const {
    std::shared_ptr<AudioData> audio_data = get_audio_data(p_id);
    if (!audio_data) {
        return 0;
    }
    return audio_data->get_frames_remaining();
}

Lowl::size_l Lowl::AudioSpace::get_frame_count(Lowl::SpaceId p_id) const {
    std::shared_ptr<AudioData> audio_data = get_audio_data(p_id);
    if (!audio_data) {
        return 0;
    }
    return audio_data->get_frame_count();
}

std::shared_ptr<Lowl::AudioData> Lowl::AudioSpace::get_audio_data(SpaceId p_id) const {
    if (p_id >= current_id) {
        return nullptr;
    }
    std::shared_ptr<AudioData> audio_data = audio_data_lookup[p_id];
    return audio_data;
}

Lowl::AudioSource::ReadResult Lowl::AudioSpace::read(Lowl::AudioFrame &audio_frame) {
    ReadResult result = mixer->read(audio_frame);
    if (result == ReadResult::Read) {
        process_volume(audio_frame);
        process_panning(audio_frame);
    }
    return result;
}

Lowl::size_l Lowl::AudioSpace::get_frames_remaining() const {
    return 1;
}

Lowl::size_l Lowl::AudioSpace::get_frame_position() const {
    return 0;
}

Lowl::size_l Lowl::AudioSpace::get_frame_count() const {
    return 0;
}

