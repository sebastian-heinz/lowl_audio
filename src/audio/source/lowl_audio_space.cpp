#include "lowl_audio_space.h"

#include <algorithm>

#include "audio/convert/lowl_audio_channel_converter.h"
#include "audio/convert/lowl_audio_re_sampler_r8b.h"
#include "audio/lowl_audio_utilities.h"
#include "audio/reader/lowl_audio_reader.h"
#include "lowl_logger.h"

Lowl::Audio::AudioSpace::AudioSpace(SampleRate p_sample_rate, AudioChannel p_channel)
    : AudioSource(p_sample_rate, p_channel) {
    mixer = std::make_unique<AudioMixer>(sample_rate, channel);
    current_id = FirstSpaceId;
    audio_data_lookup = std::vector<std::shared_ptr<AudioData>>();
    active_voice_lookup = std::vector<std::vector<std::shared_ptr<AudioVoice>>>();
}

Lowl::Audio::AudioSpace::~AudioSpace() {
}

Lowl::SpaceId Lowl::Audio::AudioSpace::insert_audio_data(std::shared_ptr<AudioData> p_audio_data) {
    std::lock_guard<std::mutex> lock(state_mutex);
    SpaceId id = current_id;
    if (audio_data_lookup.size() < id + 1) {
        audio_data_lookup.resize(id + LookupGrowth);
        active_voice_lookup.resize(id + LookupGrowth);
    }
    audio_data_lookup[id] = p_audio_data;
    current_id++;
    return id;
}

void Lowl::Audio::AudioSpace::collect_garbage_locked() const {
    for (std::vector<std::shared_ptr<AudioVoice>> &voices : active_voice_lookup) {
        voices.erase(
            std::remove_if(voices.begin(),
                           voices.end(),
                           [](const std::shared_ptr<AudioVoice> &voice) { return !voice || voice->is_detached(); }),
            voices.end());
    }

    retired_voices.erase(
        std::remove_if(retired_voices.begin(),
                       retired_voices.end(),
                       [](const std::shared_ptr<AudioVoice> &voice) { return !voice || voice->is_detached(); }),
        retired_voices.end());
}

Lowl::SpaceId Lowl::Audio::AudioSpace::add_audio(std::unique_ptr<AudioData> p_audio_data, Error &error) {
    std::shared_ptr<AudioData> audio = std::move(p_audio_data);
    SampleRate rate = audio->get_sample_rate();

    if (!Lowl::Audio::sample_rates_equal(rate, sample_rate)) {
        std::unique_ptr<AudioData> resampled = ReSamplerR8b::resample(audio, sample_rate);
        audio = std::move(resampled);
    }

    AudioChannel ch = audio->get_channel();
    if (ch != channel) {
        ChannelConverter channel_converter;
        std::unique_ptr<AudioData> converted = channel_converter.convert(channel, audio, error);
        if (error.has_error()) {
            LOWL_LOG_ERROR(
                "Lowl::Space::load channel_converter.convert() ErrCode:" + std::to_string(error.get_error_code()) +
                " ErrText:" + error.get_error_text() + ". Could not convert channels from " + std::to_string((int)ch) +
                " to " + std::to_string((int)channel) + " channel.");
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
    std::lock_guard<std::mutex> lock(state_mutex);
    collect_garbage_locked();

    for (const std::vector<std::shared_ptr<AudioVoice>> &voices : active_voice_lookup) {
        for (const std::shared_ptr<AudioVoice> &voice : voices) {
            if (voice && !voice->is_detached()) {
                mixer->remove(voice.get());
            }
        }
    }

    for (const std::vector<std::shared_ptr<AudioVoice>> &voices : active_voice_lookup) {
        for (const std::shared_ptr<AudioVoice> &voice : voices) {
            if (voice) {
                retired_voices.push_back(voice);
            }
        }
    }

    audio_data_lookup = std::vector<std::shared_ptr<AudioData>>();
    active_voice_lookup = std::vector<std::vector<std::shared_ptr<AudioVoice>>>();
    current_id = FirstSpaceId;
}

void Lowl::Audio::AudioSpace::stop_all_audio() {
    std::lock_guard<std::mutex> lock(state_mutex);
    collect_garbage_locked();
    for (const std::vector<std::shared_ptr<AudioVoice>> &voices : active_voice_lookup) {
        for (const std::shared_ptr<AudioVoice> &voice : voices) {
            if (voice && !voice->is_detached()) {
                mixer->remove(voice.get());
            }
        }
    }
}

void Lowl::Audio::AudioSpace::play(const SpaceId p_id, const Volume p_volume, const Panning p_panning) const {
    std::lock_guard<std::mutex> lock(state_mutex);
    collect_garbage_locked();
    std::shared_ptr<AudioData> audio_data = get_audio_data_locked(p_id);
    if (!audio_data) {
        return;
    }
    std::shared_ptr<AudioVoice> voice = std::make_shared<AudioVoice>(audio_data);
    voice->set_name(audio_data->get_name());
    voice->set_volume(p_volume);
    voice->set_panning(p_panning);
    active_voice_lookup[p_id].push_back(voice);
    mixer->mix(voice.get());
}

void Lowl::Audio::AudioSpace::play(const SpaceId p_id) const {
    std::lock_guard<std::mutex> lock(state_mutex);
    collect_garbage_locked();
    std::shared_ptr<AudioData> audio_data = get_audio_data_locked(p_id);
    if (!audio_data) {
        return;
    }
    std::shared_ptr<AudioVoice> voice = std::make_shared<AudioVoice>(audio_data);
    voice->set_name(audio_data->get_name());
    active_voice_lookup[p_id].push_back(voice);
    mixer->mix(voice.get());
}

void Lowl::Audio::AudioSpace::stop(const SpaceId p_id) const {
    std::lock_guard<std::mutex> lock(state_mutex);
    collect_garbage_locked();
    if (p_id >= active_voice_lookup.size()) {
        return;
    }
    for (const std::shared_ptr<AudioVoice> &voice : active_voice_lookup[p_id]) {
        if (voice && !voice->is_detached()) {
            mixer->remove(voice.get());
        }
    }
}

void Lowl::Audio::AudioSpace::set_volume(const SpaceId p_id, const Volume p_volume) const {
    std::lock_guard<std::mutex> lock(state_mutex);
    collect_garbage_locked();
    if (p_id >= active_voice_lookup.size()) {
        return;
    }
    for (const std::shared_ptr<AudioVoice> &voice : active_voice_lookup[p_id]) {
        if (voice && !voice->is_detached()) {
            voice->set_volume(p_volume);
        }
    }
}

void Lowl::Audio::AudioSpace::set_panning(const SpaceId p_id, const Panning p_panning) const {
    std::lock_guard<std::mutex> lock(state_mutex);
    collect_garbage_locked();
    if (p_id >= active_voice_lookup.size()) {
        return;
    }
    for (const std::shared_ptr<AudioVoice> &voice : active_voice_lookup[p_id]) {
        if (voice && !voice->is_detached()) {
            voice->set_panning(p_panning);
        }
    }
}

void Lowl::Audio::AudioSpace::seek_frame(const SpaceId p_id, const size_t p_frame) const {
    std::lock_guard<std::mutex> lock(state_mutex);
    collect_garbage_locked();
    if (p_id >= active_voice_lookup.size()) {
        return;
    }
    for (const std::shared_ptr<AudioVoice> &voice : active_voice_lookup[p_id]) {
        if (voice && !voice->is_detached()) {
            voice->seek_frame(p_frame);
        }
    }
}

void Lowl::Audio::AudioSpace::seek_time(const SpaceId p_id, const double_l p_seconds) const {
    std::lock_guard<std::mutex> lock(state_mutex);
    collect_garbage_locked();
    if (p_id >= active_voice_lookup.size()) {
        return;
    }
    for (const std::shared_ptr<AudioVoice> &voice : active_voice_lookup[p_id]) {
        if (voice && !voice->is_detached()) {
            voice->seek_time(p_seconds);
        }
    }
}

void Lowl::Audio::AudioSpace::reset(const SpaceId p_id) const {
    std::lock_guard<std::mutex> lock(state_mutex);
    collect_garbage_locked();
    if (p_id >= active_voice_lookup.size()) {
        return;
    }
    for (const std::shared_ptr<AudioVoice> &voice : active_voice_lookup[p_id]) {
        if (voice && !voice->is_detached()) {
            voice->reset();
        }
    }
}

Lowl::size_l Lowl::Audio::AudioSpace::get_frame_position(const SpaceId p_id) const {
    std::shared_ptr<AudioVoice> voice;
    {
        std::lock_guard<std::mutex> lock(state_mutex);
        voice = get_latest_voice_locked(p_id);
    }
    if (!voice) {
        return 0;
    }
    return voice->get_frame_position();
}

Lowl::size_l Lowl::Audio::AudioSpace::get_frames_remaining(const SpaceId p_id) const {
    std::shared_ptr<AudioVoice> voice;
    {
        std::lock_guard<std::mutex> lock(state_mutex);
        voice = get_latest_voice_locked(p_id);
    }
    if (!voice) {
        return 0;
    }
    return voice->get_frames_remaining();
}

Lowl::size_l Lowl::Audio::AudioSpace::get_frame_count(const SpaceId p_id) const {
    std::shared_ptr<AudioData> audio_data;
    {
        std::lock_guard<std::mutex> lock(state_mutex);
        audio_data = get_audio_data_locked(p_id);
    }
    if (!audio_data) {
        return 0;
    }
    return audio_data->get_frame_count();
}

std::shared_ptr<Lowl::Audio::AudioData> Lowl::Audio::AudioSpace::get_audio_data_locked(const SpaceId p_id) const {
    if (p_id >= current_id || p_id >= audio_data_lookup.size()) {
        return nullptr;
    }
    return audio_data_lookup[p_id];
}

std::shared_ptr<Lowl::Audio::AudioVoice> Lowl::Audio::AudioSpace::get_latest_voice_locked(const SpaceId p_id) const {
    if (p_id >= active_voice_lookup.size()) {
        return nullptr;
    }
    const std::vector<std::shared_ptr<AudioVoice>> &voices = active_voice_lookup[p_id];
    for (auto it = voices.rbegin(); it != voices.rend(); ++it) {
        if (*it && !(*it)->is_detached()) {
            return *it;
        }
    }
    return nullptr;
}

Lowl::Audio::AudioSource::RenderResult Lowl::Audio::AudioSpace::render(AudioBlockView p_block) {
    RenderResult result = mixer->render(p_block);
    if (result.frames_produced > 0) {
        AudioBlockView produced_block = p_block;
        produced_block.frame_count = result.frames_produced;
        process_volume(produced_block);
        process_panning(produced_block);
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
    std::lock_guard<std::mutex> lock(state_mutex);
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