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

Lowl::Audio::AudioSpace::AudioSpace(const SampleRate p_sample_rate,
                                    const AudioChannel p_channel,
                                    const uint32_t p_mixer_scratch_buffer_capacity)
    : AudioSource(p_sample_rate, p_channel) {
    mixer = std::make_unique<AudioMixer>(sample_rate, channel, p_mixer_scratch_buffer_capacity);
    mixer_owner_id = mixer->register_ack_owner();
    current_audio_asset_id = FirstAudioAssetId;
    current_audio_playback_slot_id = FirstPlaybackSlotId;
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

Lowl::AudioPlaybackHandle Lowl::Audio::AudioSpace::insert_playback_locked(std::unique_ptr<AudioVoice> p_voice,
                                                                          const AudioAssetId p_audio_asset_id) {
    if (!p_voice || current_audio_playback_slot_id == InvalidPlaybackSlotId) {
        return InvalidAudioPlaybackHandle;
    }
    const AudioPlaybackId slot_id = current_audio_playback_slot_id;
    const size_t required_size = static_cast<size_t>(slot_id) + 1;
    if (playback_lookup.size() < required_size) {
        playback_lookup.resize(required_size + LookupGrowth);
    }
    PlaybackSlot &slot = playback_lookup[slot_id];
    slot.voice = std::move(p_voice);
    slot.audio_asset_id = p_audio_asset_id;
    if (slot.generation == 0) {
        slot.generation = 1;
    }
    slot.slot_state = SlotState::Active;
    slot.voice->stop_playback();
    current_audio_playback_slot_id = advance_id(slot_id);

    AudioPlaybackHandle handle{};
    handle.id = slot_id;
    handle.generation = slot.generation;
    return handle;
}

void Lowl::Audio::AudioSpace::recycle_playback_locked(PlaybackSlot &p_slot) {
    p_slot.voice.reset();
    p_slot.audio_asset_id = InvalidAudioAssetId;
    p_slot.slot_state = SlotState::Active;
    p_slot.generation = advance_generation(p_slot.generation);
}

void Lowl::Audio::AudioSpace::drain_mixer_acks_locked() {
    AudioMixerAck ack = {};
    while (mixer->try_dequeue_ack(mixer_owner_id, ack)) {
        if (!ack.handle.is_valid() || ack.handle.owner_id != mixer_owner_id) {
            continue;
        }
        const AudioPlaybackId playback_id = ack.handle.playback_id;
        if (playback_id >= playback_lookup.size()) {
            continue;
        }
        PlaybackSlot &slot = playback_lookup[playback_id];
        if (slot.generation != ack.handle.generation) {
            continue;
        }

        switch (ack.type) {
            case AudioMixerAck::Type::Removed: {
                if (slot.slot_state == SlotState::Retiring) {
                    recycle_playback_locked(slot);
                }
                break;
            }
            case AudioMixerAck::Type::Finished: {
                if (!slot.voice) {
                    break;
                }
                slot.voice->stop_playback();
                if (slot.slot_state == SlotState::Retiring) {
                    recycle_playback_locked(slot);
                }
                break;
            }
            case AudioMixerAck::Type::Rejected: {
                if (!slot.voice) {
                    break;
                }
                if (slot.slot_state == SlotState::Retiring) {
                    recycle_playback_locked(slot);
                } else if (slot.voice->get_playback_state() == AudioVoice::PlaybackState::Playing) {
                    slot.voice->pause_playback();
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

Lowl::AudioPlaybackHandle Lowl::Audio::AudioSpace::create_playback(const AudioAssetId p_audio_asset_id) {
    std::lock_guard<std::mutex> lock(state_mutex);
    drain_mixer_acks_locked();
    std::shared_ptr<AudioData> audio_data = get_audio_asset_locked(p_audio_asset_id);
    if (!audio_data) {
        return InvalidAudioPlaybackHandle;
    }
    std::unique_ptr<AudioVoice> voice = std::make_unique<AudioVoice>(audio_data);
    voice->set_name(audio_data->get_name());
    return insert_playback_locked(std::move(voice), p_audio_asset_id);
}

void Lowl::Audio::AudioSpace::clear_all_audio() {
    std::lock_guard<std::mutex> lock(state_mutex);
    drain_mixer_acks_locked();

    for (AudioPlaybackId playback_slot_id = FirstPlaybackSlotId; playback_slot_id < playback_lookup.size();
         playback_slot_id++) {
        PlaybackSlot &slot = playback_lookup[playback_slot_id];
        if (!slot.voice || slot.slot_state == SlotState::Retiring) {
            continue;
        }
        slot.voice->stop_playback();
        slot.slot_state = SlotState::Retiring;
        AudioPlaybackHandle handle{};
        handle.id = playback_slot_id;
        handle.generation = slot.generation;
        mixer->remove(get_mixer_handle_locked(handle), true);
    }

    audio_asset_lookup.clear();
}

void Lowl::Audio::AudioSpace::stop_all_audio() {
    std::lock_guard<std::mutex> lock(state_mutex);
    drain_mixer_acks_locked();
    for (AudioPlaybackId playback_slot_id = FirstPlaybackSlotId; playback_slot_id < playback_lookup.size();
         playback_slot_id++) {
        AudioPlaybackHandle handle{};
        handle.id = playback_slot_id;
        handle.generation = playback_lookup[playback_slot_id].generation;
        PlaybackSlot *slot = get_playback_slot_locked(handle);
        if (!slot) {
            continue;
        }
        AudioVoice *voice = slot->voice.get();
        voice->stop_playback();
        mixer->remove(get_mixer_handle_locked(handle));
    }
}

void Lowl::Audio::AudioSpace::play(AudioPlaybackHandle p_audio_playback_handle) {
    std::lock_guard<std::mutex> lock(state_mutex);
    drain_mixer_acks_locked();
    PlaybackSlot *slot = get_playback_slot_locked(p_audio_playback_handle);
    if (!slot) {
        return;
    }
    AudioVoice *voice = slot->voice.get();
    voice->restart_playback();
    mixer->mix(get_mixer_handle_locked(p_audio_playback_handle), voice);
}

void Lowl::Audio::AudioSpace::pause(AudioPlaybackHandle p_audio_playback_handle) {
    std::lock_guard<std::mutex> lock(state_mutex);
    PlaybackSlot *slot = get_playback_slot_locked(p_audio_playback_handle);
    if (!slot) {
        return;
    }
    AudioVoice *voice = slot->voice.get();
    voice->pause_playback();
    mixer->remove(get_mixer_handle_locked(p_audio_playback_handle));
}

void Lowl::Audio::AudioSpace::resume(AudioPlaybackHandle p_audio_playback_handle) {
    std::lock_guard<std::mutex> lock(state_mutex);
    drain_mixer_acks_locked();
    PlaybackSlot *slot = get_playback_slot_locked(p_audio_playback_handle);
    if (!slot || slot->voice->get_playback_state() != AudioVoice::PlaybackState::Paused) {
        return;
    }
    AudioVoice *voice = slot->voice.get();
    voice->resume_playback();
    mixer->mix(get_mixer_handle_locked(p_audio_playback_handle), voice);
}

void Lowl::Audio::AudioSpace::stop(AudioPlaybackHandle p_audio_playback_handle) {
    std::lock_guard<std::mutex> lock(state_mutex);
    PlaybackSlot *slot = get_playback_slot_locked(p_audio_playback_handle);
    if (!slot) {
        return;
    }
    AudioVoice *voice = slot->voice.get();
    voice->stop_playback();
    mixer->remove(get_mixer_handle_locked(p_audio_playback_handle));
}

void Lowl::Audio::AudioSpace::set_volume(AudioPlaybackHandle p_audio_playback_handle, const Volume p_volume) {
    std::lock_guard<std::mutex> lock(state_mutex);
    PlaybackSlot *slot = get_playback_slot_locked(p_audio_playback_handle);
    AudioVoice *voice = slot ? slot->voice.get() : nullptr;
    if (!voice) {
        return;
    }
    voice->set_volume(p_volume);
}

void Lowl::Audio::AudioSpace::set_panning(AudioPlaybackHandle p_audio_playback_handle, const Panning p_panning) {
    std::lock_guard<std::mutex> lock(state_mutex);
    PlaybackSlot *slot = get_playback_slot_locked(p_audio_playback_handle);
    AudioVoice *voice = slot ? slot->voice.get() : nullptr;
    if (!voice) {
        return;
    }
    voice->set_panning(p_panning);
}

void Lowl::Audio::AudioSpace::seek_frame(AudioPlaybackHandle p_audio_playback_handle, const size_t p_frame) {
    std::lock_guard<std::mutex> lock(state_mutex);
    PlaybackSlot *slot = get_playback_slot_locked(p_audio_playback_handle);
    AudioVoice *voice = slot ? slot->voice.get() : nullptr;
    if (!voice) {
        return;
    }
    voice->seek_frame(p_frame);
}

void Lowl::Audio::AudioSpace::seek_time(AudioPlaybackHandle p_audio_playback_handle, const double_l p_seconds) {
    std::lock_guard<std::mutex> lock(state_mutex);
    PlaybackSlot *slot = get_playback_slot_locked(p_audio_playback_handle);
    AudioVoice *voice = slot ? slot->voice.get() : nullptr;
    if (!voice) {
        return;
    }
    voice->seek_time(p_seconds);
}

void Lowl::Audio::AudioSpace::reset(AudioPlaybackHandle p_audio_playback_handle) {
    std::lock_guard<std::mutex> lock(state_mutex);
    PlaybackSlot *slot = get_playback_slot_locked(p_audio_playback_handle);
    AudioVoice *voice = slot ? slot->voice.get() : nullptr;
    if (!voice) {
        return;
    }
    voice->reset();
}

Lowl::size_l Lowl::Audio::AudioSpace::get_frame_position(AudioPlaybackHandle p_audio_playback_handle) const {
    std::lock_guard<std::mutex> lock(state_mutex);
    const PlaybackSlot *slot = get_playback_slot_locked(p_audio_playback_handle);
    const AudioVoice *voice = slot ? slot->voice.get() : nullptr;
    if (!voice) {
        return 0;
    }
    return voice->get_frame_position();
}

Lowl::size_l Lowl::Audio::AudioSpace::get_frames_remaining(AudioPlaybackHandle p_audio_playback_handle) const {
    std::lock_guard<std::mutex> lock(state_mutex);
    const PlaybackSlot *slot = get_playback_slot_locked(p_audio_playback_handle);
    const AudioVoice *voice = slot ? slot->voice.get() : nullptr;
    if (!voice) {
        return 0;
    }
    return voice->get_frames_remaining();
}

Lowl::size_l Lowl::Audio::AudioSpace::get_frame_count(AudioPlaybackHandle p_audio_playback_handle) const {
    std::lock_guard<std::mutex> lock(state_mutex);
    const PlaybackSlot *slot = get_playback_slot_locked(p_audio_playback_handle);
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

Lowl::AudioMixerHandle
Lowl::Audio::AudioSpace::get_mixer_handle_locked(const AudioPlaybackHandle p_audio_playback_handle) const {
    if (!p_audio_playback_handle.is_valid() || p_audio_playback_handle.id >= playback_lookup.size()) {
        return {};
    }
    const PlaybackSlot &slot = playback_lookup[p_audio_playback_handle.id];
    if (slot.generation == 0 || slot.generation != p_audio_playback_handle.generation) {
        return {};
    }
    AudioMixerHandle handle{};
    handle.owner_id = mixer_owner_id;
    handle.playback_id = p_audio_playback_handle.id;
    handle.generation = slot.generation;
    return handle;
}

Lowl::Audio::AudioSpace::PlaybackSlot *
Lowl::Audio::AudioSpace::get_playback_slot_locked(const AudioPlaybackHandle p_audio_playback_handle) {
    if (!p_audio_playback_handle.is_valid() || p_audio_playback_handle.id >= playback_lookup.size()) {
        return nullptr;
    }
    PlaybackSlot &slot = playback_lookup[p_audio_playback_handle.id];
    if (!slot.voice || slot.slot_state == SlotState::Retiring ||
        slot.generation != p_audio_playback_handle.generation) {
        return nullptr;
    }
    return &slot;
}

const Lowl::Audio::AudioSpace::PlaybackSlot *
Lowl::Audio::AudioSpace::get_playback_slot_locked(const AudioPlaybackHandle p_audio_playback_handle) const {
    if (!p_audio_playback_handle.is_valid() || p_audio_playback_handle.id >= playback_lookup.size()) {
        return nullptr;
    }
    const PlaybackSlot &slot = playback_lookup[p_audio_playback_handle.id];
    if (!slot.voice || slot.slot_state == SlotState::Retiring ||
        slot.generation != p_audio_playback_handle.generation) {
        return nullptr;
    }
    return &slot;
}

Lowl::Audio::AudioSource::RenderResult Lowl::Audio::AudioSpace::render(AudioBlockView p_block) {
    if (!playback_enabled.load(std::memory_order_relaxed)) {
        return {0, RenderState::Starved};
    }
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
