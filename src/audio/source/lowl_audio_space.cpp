#include "lowl_audio_space.h"

#include <algorithm>
#include <limits>

#include "./lowl_logger.h"
#include "audio/convert/lowl_audio_channel_converter.h"
#include "audio/convert/lowl_audio_re_sampler_r8b.h"
#include "audio/lowl_audio_utilities.h"
#include "audio/reader/lowl_audio_reader.h"

namespace {
    template <typename T>
    T advance_id(const T p_current) {
        return p_current == std::numeric_limits<T>::max() ? 0 : static_cast<T>(p_current + 1);
    }

    Lowl::uint16_l advance_generation(const Lowl::uint16_l p_current) {
        return p_current == std::numeric_limits<Lowl::uint16_l>::max() ? 1 : static_cast<Lowl::uint16_l>(p_current + 1);
    }
} // namespace

Lowl::Audio::AudioSpace::AudioSpace(SampleRate p_sample_rate, AudioChannel p_channel)
    : AudioSource(p_sample_rate, p_channel) {
    mixer = std::make_unique<AudioMixer>(sample_rate, channel);
    mixer_owner_id = mixer->register_ack_owner();
    current_audio_asset_id = FirstAudioAssetId;
    current_audio_playback_id = FirstAudioPlaybackId;
    audio_asset_lookup = std::vector<std::shared_ptr<AudioData>>();
    playback_lookup = std::vector<PlaybackSlot>();
}

Lowl::Audio::AudioSpace::~AudioSpace() {
    if (mixer && mixer_owner_id != 0) {
        mixer->unregister_ack_owner(mixer_owner_id);
    }
}

Lowl::AudioAssetId Lowl::Audio::AudioSpace::insert_audio_asset_locked(std::shared_ptr<AudioData> p_audio_data) {
    if (current_audio_asset_id == InvalidAudioAssetId) {
        return InvalidAudioAssetId;
    }
    AudioAssetId id = current_audio_asset_id;
    const size_t required_size = static_cast<size_t>(id) + 1;
    if (audio_asset_lookup.size() < required_size) {
        audio_asset_lookup.resize(required_size + LookupGrowth);
    }
    audio_asset_lookup[id] = std::move(p_audio_data);
    current_audio_asset_id = advance_id(id);
    return id;
}

Lowl::AudioPlaybackId Lowl::Audio::AudioSpace::insert_playback_locked(std::unique_ptr<AudioVoice> p_voice,
                                                                      const AudioAssetId p_audio_asset_id) {
    if (!p_voice || current_audio_playback_id == InvalidAudioPlaybackId) {
        return InvalidAudioPlaybackId;
    }
    AudioPlaybackId id = current_audio_playback_id;
    const size_t required_size = static_cast<size_t>(id) + 1;
    if (playback_lookup.size() < required_size) {
        playback_lookup.resize(required_size + LookupGrowth);
    }
    PlaybackSlot &slot = playback_lookup[id];
    slot.voice = std::move(p_voice);
    slot.audio_asset_id = p_audio_asset_id;
    if (slot.generation == 0) {
        slot.generation = 1;
    }
    slot.state = PlaybackState::Stopped;
    slot.voice->pause();
    current_audio_playback_id = advance_id(id);
    return id;
}

void Lowl::Audio::AudioSpace::recycle_playback_locked(PlaybackSlot &p_slot) {
    p_slot.voice.reset();
    p_slot.audio_asset_id = InvalidAudioAssetId;
    p_slot.state = PlaybackState::Free;
    p_slot.generation = advance_generation(p_slot.generation);
}

void Lowl::Audio::AudioSpace::collect_garbage_locked() {
    AudioMixerAck ack = {};
    while (mixer->try_dequeue_ack(mixer_owner_id, ack)) {
        if (!ack.handle.is_valid() || ack.handle.owner_id != mixer_owner_id) {
            continue;
        }
        const AudioPlaybackId playback_id = ack.handle.slot_index;
        if (playback_id >= playback_lookup.size()) {
            continue;
        }
        PlaybackSlot &slot = playback_lookup[playback_id];
        if (slot.generation != ack.handle.generation) {
            continue;
        }

        switch (ack.type) {
            case AudioMixerAck::Type::Removed: {
                if (slot.state == PlaybackState::Retiring) {
                    recycle_playback_locked(slot);
                }
                break;
            }
            case AudioMixerAck::Type::Finished: {
                if (!slot.voice) {
                    break;
                }
                slot.voice->pause();
                slot.voice->reset();
                if (slot.state == PlaybackState::Retiring) {
                    recycle_playback_locked(slot);
                } else {
                    slot.state = PlaybackState::Stopped;
                }
                break;
            }
            case AudioMixerAck::Type::Rejected: {
                if (!slot.voice) {
                    break;
                }
                if (slot.state == PlaybackState::Retiring) {
                    recycle_playback_locked(slot);
                } else if (slot.state == PlaybackState::Playing) {
                    slot.voice->pause();
                    slot.state = PlaybackState::Paused;
                }
                break;
            }
        }
    }
}

Lowl::AudioAssetId Lowl::Audio::AudioSpace::add_audio(std::unique_ptr<AudioData> p_audio_data, Error &error) {
    std::shared_ptr<AudioData> audio = std::move(p_audio_data);
    if (!audio) {
        return InvalidAudioAssetId;
    }
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
            return InvalidAudioAssetId;
        }
        audio = std::move(converted);
    }

    std::lock_guard<std::mutex> lock(state_mutex);
    return insert_audio_asset_locked(std::move(audio));
}

Lowl::AudioAssetId Lowl::Audio::AudioSpace::add_audio(const std::string &p_path, Error &error) {
    std::unique_ptr<AudioData> audio_data = AudioReader::create_data(p_path, error);
    if (error.has_error()) {
        return InvalidAudioAssetId;
    }
    return add_audio(std::move(audio_data), error);
}

Lowl::AudioPlaybackId Lowl::Audio::AudioSpace::create_playback(const AudioAssetId p_audio_asset_id) {
    std::lock_guard<std::mutex> lock(state_mutex);
    collect_garbage_locked();
    std::shared_ptr<AudioData> audio_data = get_audio_asset_locked(p_audio_asset_id);
    if (!audio_data) {
        return InvalidAudioPlaybackId;
    }
    std::unique_ptr<AudioVoice> voice = std::make_unique<AudioVoice>(audio_data);
    voice->set_name(audio_data->get_name());
    return insert_playback_locked(std::move(voice), p_audio_asset_id);
}

void Lowl::Audio::AudioSpace::clear_all_audio() {
    std::lock_guard<std::mutex> lock(state_mutex);
    collect_garbage_locked();

    for (AudioPlaybackId playback_id = FirstAudioPlaybackId; playback_id < playback_lookup.size(); playback_id++) {
        PlaybackSlot &slot = playback_lookup[playback_id];
        if (!slot.voice || slot.state == PlaybackState::Free || slot.state == PlaybackState::Retiring) {
            continue;
        }
        slot.voice->pause();
        slot.voice->reset();
        slot.state = PlaybackState::Retiring;
        mixer->remove(get_mixer_handle_locked(playback_id), true);
    }

    audio_asset_lookup.clear();
}

void Lowl::Audio::AudioSpace::stop_all_audio() {
    std::lock_guard<std::mutex> lock(state_mutex);
    collect_garbage_locked();
    for (AudioPlaybackId playback_id = FirstAudioPlaybackId; playback_id < playback_lookup.size(); playback_id++) {
        PlaybackSlot *slot = get_playback_slot_locked(playback_id);
        if (!slot) {
            continue;
        }
        AudioVoice *voice = slot->voice.get();
        voice->pause();
        voice->reset();
        slot->state = PlaybackState::Stopped;
        mixer->remove(get_mixer_handle_locked(playback_id));
    }
}

void Lowl::Audio::AudioSpace::play(const AudioPlaybackId p_audio_playback_id) {
    std::lock_guard<std::mutex> lock(state_mutex);
    collect_garbage_locked();
    PlaybackSlot *slot = get_playback_slot_locked(p_audio_playback_id);
    if (!slot) {
        return;
    }
    AudioVoice *voice = slot->voice.get();
    voice->reset();
    voice->play();
    slot->state = PlaybackState::Playing;
    mixer->mix(get_mixer_handle_locked(p_audio_playback_id), voice);
}

void Lowl::Audio::AudioSpace::pause(const AudioPlaybackId p_audio_playback_id) {
    std::lock_guard<std::mutex> lock(state_mutex);
    collect_garbage_locked();
    PlaybackSlot *slot = get_playback_slot_locked(p_audio_playback_id);
    if (!slot) {
        return;
    }
    AudioVoice *voice = slot->voice.get();
    voice->pause();
    slot->state = PlaybackState::Paused;
    mixer->remove(get_mixer_handle_locked(p_audio_playback_id));
}

void Lowl::Audio::AudioSpace::resume(const AudioPlaybackId p_audio_playback_id) {
    std::lock_guard<std::mutex> lock(state_mutex);
    collect_garbage_locked();
    PlaybackSlot *slot = get_playback_slot_locked(p_audio_playback_id);
    if (!slot || slot->state != PlaybackState::Paused) {
        return;
    }
    AudioVoice *voice = slot->voice.get();
    voice->play();
    slot->state = PlaybackState::Playing;
    mixer->mix(get_mixer_handle_locked(p_audio_playback_id), voice);
}

void Lowl::Audio::AudioSpace::stop(const AudioPlaybackId p_audio_playback_id) {
    std::lock_guard<std::mutex> lock(state_mutex);
    collect_garbage_locked();
    PlaybackSlot *slot = get_playback_slot_locked(p_audio_playback_id);
    if (!slot) {
        return;
    }
    AudioVoice *voice = slot->voice.get();
    voice->pause();
    voice->reset();
    slot->state = PlaybackState::Stopped;
    mixer->remove(get_mixer_handle_locked(p_audio_playback_id));
}

void Lowl::Audio::AudioSpace::set_volume(const AudioPlaybackId p_audio_playback_id, const Volume p_volume) {
    std::lock_guard<std::mutex> lock(state_mutex);
    collect_garbage_locked();
    PlaybackSlot *slot = get_playback_slot_locked(p_audio_playback_id);
    AudioVoice *voice = slot ? slot->voice.get() : nullptr;
    if (!voice) {
        return;
    }
    voice->set_volume(p_volume);
}

void Lowl::Audio::AudioSpace::set_panning(const AudioPlaybackId p_audio_playback_id, const Panning p_panning) {
    std::lock_guard<std::mutex> lock(state_mutex);
    collect_garbage_locked();
    PlaybackSlot *slot = get_playback_slot_locked(p_audio_playback_id);
    AudioVoice *voice = slot ? slot->voice.get() : nullptr;
    if (!voice) {
        return;
    }
    voice->set_panning(p_panning);
}

void Lowl::Audio::AudioSpace::seek_frame(const AudioPlaybackId p_audio_playback_id, const size_t p_frame) {
    std::lock_guard<std::mutex> lock(state_mutex);
    collect_garbage_locked();
    PlaybackSlot *slot = get_playback_slot_locked(p_audio_playback_id);
    AudioVoice *voice = slot ? slot->voice.get() : nullptr;
    if (!voice) {
        return;
    }
    voice->seek_frame(p_frame);
}

void Lowl::Audio::AudioSpace::seek_time(const AudioPlaybackId p_audio_playback_id, const double_l p_seconds) {
    std::lock_guard<std::mutex> lock(state_mutex);
    collect_garbage_locked();
    PlaybackSlot *slot = get_playback_slot_locked(p_audio_playback_id);
    AudioVoice *voice = slot ? slot->voice.get() : nullptr;
    if (!voice) {
        return;
    }
    voice->seek_time(p_seconds);
}

void Lowl::Audio::AudioSpace::reset(const AudioPlaybackId p_audio_playback_id) {
    std::lock_guard<std::mutex> lock(state_mutex);
    collect_garbage_locked();
    PlaybackSlot *slot = get_playback_slot_locked(p_audio_playback_id);
    AudioVoice *voice = slot ? slot->voice.get() : nullptr;
    if (!voice) {
        return;
    }
    voice->reset();
}

Lowl::size_l Lowl::Audio::AudioSpace::get_frame_position(const AudioPlaybackId p_audio_playback_id) const {
    std::lock_guard<std::mutex> lock(state_mutex);
    const PlaybackSlot *slot = get_playback_slot_locked(p_audio_playback_id);
    const AudioVoice *voice = slot ? slot->voice.get() : nullptr;
    if (!voice) {
        return 0;
    }
    return voice->get_frame_position();
}

Lowl::size_l Lowl::Audio::AudioSpace::get_frames_remaining(const AudioPlaybackId p_audio_playback_id) const {
    std::lock_guard<std::mutex> lock(state_mutex);
    const PlaybackSlot *slot = get_playback_slot_locked(p_audio_playback_id);
    const AudioVoice *voice = slot ? slot->voice.get() : nullptr;
    if (!voice) {
        return 0;
    }
    return voice->get_frames_remaining();
}

Lowl::size_l Lowl::Audio::AudioSpace::get_frame_count(const AudioPlaybackId p_audio_playback_id) const {
    std::lock_guard<std::mutex> lock(state_mutex);
    const PlaybackSlot *slot = get_playback_slot_locked(p_audio_playback_id);
    const AudioVoice *voice = slot ? slot->voice.get() : nullptr;
    if (!voice) {
        return 0;
    }
    return voice->get_frame_count();
}

std::shared_ptr<Lowl::Audio::AudioData>
Lowl::Audio::AudioSpace::get_audio_asset_locked(const AudioAssetId p_audio_asset_id) const {
    if (p_audio_asset_id == InvalidAudioAssetId || p_audio_asset_id >= audio_asset_lookup.size()) {
        return nullptr;
    }
    return audio_asset_lookup[p_audio_asset_id];
}

Lowl::AudioMixerHandle Lowl::Audio::AudioSpace::get_mixer_handle_locked(const AudioPlaybackId p_audio_playback_id) const {
    if (p_audio_playback_id == InvalidAudioPlaybackId || p_audio_playback_id >= playback_lookup.size()) {
        return {};
    }
    const PlaybackSlot &slot = playback_lookup[p_audio_playback_id];
    if (slot.generation == 0) {
        return {};
    }
    AudioMixerHandle handle{};
    handle.owner_id = mixer_owner_id;
    handle.slot_index = p_audio_playback_id;
    handle.generation = slot.generation;
    return handle;
}

Lowl::Audio::AudioSpace::PlaybackSlot *
Lowl::Audio::AudioSpace::get_playback_slot_locked(const AudioPlaybackId p_audio_playback_id) {
    if (p_audio_playback_id == InvalidAudioPlaybackId || p_audio_playback_id >= playback_lookup.size()) {
        return nullptr;
    }
    PlaybackSlot &slot = playback_lookup[p_audio_playback_id];
    if (!slot.voice || slot.state == PlaybackState::Free || slot.state == PlaybackState::Retiring) {
        return nullptr;
    }
    return &slot;
}

const Lowl::Audio::AudioSpace::PlaybackSlot *
Lowl::Audio::AudioSpace::get_playback_slot_locked(const AudioPlaybackId p_audio_playback_id) const {
    if (p_audio_playback_id == InvalidAudioPlaybackId || p_audio_playback_id >= playback_lookup.size()) {
        return nullptr;
    }
    const PlaybackSlot &slot = playback_lookup[p_audio_playback_id];
    if (!slot.voice || slot.state == PlaybackState::Free || slot.state == PlaybackState::Retiring) {
        return nullptr;
    }
    return &slot;
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

std::map<Lowl::AudioAssetId, std::string> Lowl::Audio::AudioSpace::get_name_mapping() const {
    std::lock_guard<std::mutex> lock(state_mutex);
    std::map<AudioAssetId, std::string> map = std::map<AudioAssetId, std::string>();
    for (AudioAssetId audio_asset_id = 0; audio_asset_id < audio_asset_lookup.size(); audio_asset_id++) {
        std::shared_ptr<AudioData> audio_data = audio_asset_lookup[audio_asset_id];
        if (!audio_data) {
            continue;
        }
        map.insert_or_assign(audio_asset_id, audio_data->get_name());
    }
    return map;
}
