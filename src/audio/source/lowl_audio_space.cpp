#include "lowl_audio_space.h"

#include <algorithm>
#include <atomic>
#include <limits>
#include <utility>

#include "audio/convert/lowl_audio_channel_converter.h"
#include "audio/convert/lowl_audio_re_sampler_r8b.h"
#include "audio/lowl_audio_utilities.h"
#include "audio/reader/lowl_audio_reader.h"
#include "lowl_logger.h"

namespace {
    std::atomic<Lowl::uint32_l> next_audio_space_owner_id{1};

    template <typename T>
    T advance_allocation_id(const T p_current) {
        return p_current == std::numeric_limits<T>::max() ? static_cast<T>(0) : static_cast<T>(p_current + 1);
    }

    Lowl::uint16_l advance_generation(const Lowl::uint16_l p_current) {
        return p_current == std::numeric_limits<Lowl::uint16_l>::max() ? 1 : static_cast<Lowl::uint16_l>(p_current + 1);
    }

    Lowl::uint32_l allocate_audio_space_owner_id() {
        Lowl::uint32_l owner_id = next_audio_space_owner_id.fetch_add(1, std::memory_order_relaxed);
        if (owner_id == 0) {
            owner_id = next_audio_space_owner_id.fetch_add(1, std::memory_order_relaxed);
        }
        return owner_id;
    }
} // namespace

Lowl::Audio::AudioSpace::AudioSpace(const AudioFormat p_audio_format)
    : AudioSource(p_audio_format),
      mixer(p_audio_format),
      owner_id(allocate_audio_space_owner_id()),
      current_audio_asset_id(FirstAudioAssetId),
      current_audio_playback_slot_id(FirstPlaybackSlotId) {
}

Lowl::AudioAssetHandle
Lowl::Audio::AudioSpace::insert_audio_asset_locked(std::shared_ptr<AudioData> p_audio_data) {
    if (!p_audio_data) {
        return InvalidAudioAssetHandle;
    }

    AudioAssetId asset_id = InvalidAudioAssetId;
    const bool reusing_slot = !free_audio_asset_slots.empty();
    if (reusing_slot) {
        asset_id = free_audio_asset_slots.back();
    } else {
        if (current_audio_asset_id == InvalidAudioAssetId) {
            return InvalidAudioAssetHandle;
        }
        asset_id = current_audio_asset_id;
        const size_t required_size = static_cast<size_t>(asset_id) + 1;
        if (audio_asset_lookup.size() < required_size) {
            const size_t max_lookup_size = static_cast<size_t>(std::numeric_limits<AudioAssetId>::max()) + 1;
            audio_asset_lookup.resize(std::min(required_size + LookupGrowth, max_lookup_size));
        }
    }

    if (asset_id == InvalidAudioAssetId || asset_id >= audio_asset_lookup.size()) {
        return InvalidAudioAssetHandle;
    }
    if (reusing_slot) {
        free_audio_asset_slots.pop_back();
    } else {
        current_audio_asset_id = advance_allocation_id(asset_id);
    }

    AssetSlot &slot = audio_asset_lookup[asset_id];
    slot.audio_data = std::move(p_audio_data);
    if (slot.generation == 0) {
        slot.generation = 1;
    }
    return {owner_id, asset_id, slot.generation};
}

Lowl::AudioPlaybackHandle
Lowl::Audio::AudioSpace::insert_playback_locked(std::unique_ptr<AudioVoice> p_voice,
                                                const AudioAssetHandle p_audio_asset_handle) {
    if (!p_voice) {
        return InvalidAudioPlaybackHandle;
    }

    const AudioMixerHandle mixer_handle = mixer.allocate_handle();
    if (!mixer_handle.is_valid()) {
        return InvalidAudioPlaybackHandle;
    }

    AudioPlaybackId playback_id = InvalidPlaybackSlotId;
    const bool reusing_slot = !free_playback_slots.empty();
    if (reusing_slot) {
        playback_id = free_playback_slots.back();
    } else {
        if (current_audio_playback_slot_id == InvalidPlaybackSlotId) {
            mixer.release_handle(mixer_handle);
            return InvalidAudioPlaybackHandle;
        }
        playback_id = current_audio_playback_slot_id;
        const size_t required_size = static_cast<size_t>(playback_id) + 1;
        if (playback_lookup.size() < required_size) {
            const size_t max_lookup_size = static_cast<size_t>(std::numeric_limits<AudioPlaybackId>::max()) + 1;
            playback_lookup.resize(std::min(required_size + LookupGrowth, max_lookup_size));
        }
    }

    if (playback_id == InvalidPlaybackSlotId || playback_id >= playback_lookup.size()) {
        mixer.release_handle(mixer_handle);
        return InvalidAudioPlaybackHandle;
    }
    if (reusing_slot) {
        free_playback_slots.pop_back();
    } else {
        current_audio_playback_slot_id = advance_allocation_id(playback_id);
    }

    const size_t mixer_lookup_size = static_cast<size_t>(mixer_handle.playback_id) + 1;
    if (mixer_playback_lookup.size() < mixer_lookup_size) {
        mixer_playback_lookup.resize(mixer_lookup_size, InvalidPlaybackSlotId);
    }

    PlaybackSlot &slot = playback_lookup[playback_id];
    slot.voice = std::move(p_voice);
    slot.audio_asset_handle = p_audio_asset_handle;
    slot.mixer_handle = mixer_handle;
    if (slot.generation == 0) {
        slot.generation = 1;
    }
    slot.slot_state = SlotState::Active;
    slot.mixer_submission_started = false;
    slot.retirement_retry_needed = false;
    mixer_playback_lookup[mixer_handle.playback_id] = playback_id;
    return {owner_id, playback_id, slot.generation};
}

void Lowl::Audio::AudioSpace::recycle_audio_asset_locked(const AudioAssetId p_asset_id, AssetSlot &p_slot) {
    p_slot.audio_data.reset();
    p_slot.generation = advance_generation(p_slot.generation);
    if (p_asset_id != InvalidAudioAssetId) {
        free_audio_asset_slots.push_back(p_asset_id);
    }
}

void Lowl::Audio::AudioSpace::recycle_playback_locked(const AudioPlaybackId p_slot_id, PlaybackSlot &p_slot) {
    if (p_slot.mixer_handle.is_valid()) {
        const AudioPlaybackId mixer_id = p_slot.mixer_handle.playback_id;
        if (mixer_id < mixer_playback_lookup.size() && mixer_playback_lookup[mixer_id] == p_slot_id) {
            mixer_playback_lookup[mixer_id] = InvalidPlaybackSlotId;
        }
        mixer.release_handle(p_slot.mixer_handle);
    }

    p_slot.voice.reset();
    p_slot.audio_asset_handle = InvalidAudioAssetHandle;
    p_slot.mixer_handle = {};
    p_slot.slot_state = SlotState::Active;
    p_slot.mixer_submission_started = false;
    p_slot.retirement_retry_needed = false;
    p_slot.generation = advance_generation(p_slot.generation);
    if (p_slot_id != InvalidPlaybackSlotId) {
        free_playback_slots.push_back(p_slot_id);
    }
}

void Lowl::Audio::AudioSpace::retire_playback_locked(const AudioPlaybackId p_slot_id, PlaybackSlot &p_slot) {
    if (!p_slot.voice || p_slot.slot_state == SlotState::Retiring) {
        return;
    }

    const bool can_recycle_immediately =
        !p_slot.mixer_handle.is_valid() || !p_slot.mixer_submission_started ||
        (p_slot.voice->is_detached() &&
         p_slot.voice->get_playback_state() != AudioVoice::PlaybackState::Playing);
    if (can_recycle_immediately) {
        recycle_playback_locked(p_slot_id, p_slot);
        return;
    }

    p_slot.voice->stop_playback();
    p_slot.slot_state = SlotState::Retiring;
    p_slot.retirement_retry_needed = false;
    mixer.remove(p_slot.mixer_handle, true);
}

Lowl::AudioPlaybackId
Lowl::Audio::AudioSpace::find_playback_slot_id_by_mixer_handle_locked(const AudioMixerHandle p_mixer_handle) const {
    if (!p_mixer_handle.is_valid() || p_mixer_handle.playback_id >= mixer_playback_lookup.size()) {
        return InvalidPlaybackSlotId;
    }
    const AudioPlaybackId playback_id = mixer_playback_lookup[p_mixer_handle.playback_id];
    if (playback_id == InvalidPlaybackSlotId || playback_id >= playback_lookup.size()) {
        return InvalidPlaybackSlotId;
    }
    const PlaybackSlot &slot = playback_lookup[playback_id];
    return slot.voice && slot.mixer_handle == p_mixer_handle ? playback_id : InvalidPlaybackSlotId;
}

void Lowl::Audio::AudioSpace::drain_mixer_acks_locked() {
    AudioMixerAck ack{};
    while (mixer.try_dequeue_ack(ack)) {
        const AudioPlaybackId playback_id = find_playback_slot_id_by_mixer_handle_locked(ack.handle);
        if (playback_id == InvalidPlaybackSlotId) {
            continue;
        }

        PlaybackSlot &slot = playback_lookup[playback_id];
        switch (ack.type) {
            case AudioMixerAck::Type::Removed: {
                if (slot.slot_state == SlotState::Retiring) {
                    recycle_playback_locked(playback_id, slot);
                }
                break;
            }
            case AudioMixerAck::Type::Finished: {
                if (!slot.voice) {
                    break;
                }
                slot.voice->stop_playback();
                if (slot.slot_state == SlotState::Retiring) {
                    recycle_playback_locked(playback_id, slot);
                }
                break;
            }
            case AudioMixerAck::Type::Rejected: {
                if (!slot.voice) {
                    break;
                }
                if (slot.slot_state == SlotState::Retiring) {
                    slot.retirement_retry_needed = true;
                    retirement_retry_slots.push_back(playback_id);
                } else if (slot.voice->get_playback_state() == AudioVoice::PlaybackState::Playing) {
                    slot.voice->pause_playback();
                }
                break;
            }
        }
    }

    for (const AudioPlaybackId playback_id : retirement_retry_slots) {
        if (playback_id == InvalidPlaybackSlotId || playback_id >= playback_lookup.size()) {
            continue;
        }
        PlaybackSlot &slot = playback_lookup[playback_id];
        if (!slot.voice || slot.slot_state != SlotState::Retiring || !slot.retirement_retry_needed) {
            continue;
        }
        slot.retirement_retry_needed = false;
        mixer.remove(slot.mixer_handle, true);
    }
    retirement_retry_slots.clear();
}

Lowl::AudioAssetHandle Lowl::Audio::AudioSpace::add_audio(std::unique_ptr<AudioData> p_audio_data, Error &p_error) {
    std::shared_ptr<AudioData> audio = std::move(p_audio_data);
    if (!audio) {
        return InvalidAudioAssetHandle;
    }

    const AudioFormat &space_format = get_audio_format();
    if (!sample_rates_equal(audio->get_audio_format().sample_rate, space_format.sample_rate)) {
        std::unique_ptr<AudioData> resampled = ReSamplerR8b::resample(audio, space_format.sample_rate);
        if (!resampled) {
            p_error.set_error(ErrorCode::Error);
            LOWL_LOG_ERROR("Lowl::AudioSpace::add_audio: resampling failed.");
            return InvalidAudioAssetHandle;
        }
        audio = std::move(resampled);
    }

    const ChannelLayout source_layout = audio->get_audio_format().channel_layout;
    if (source_layout != space_format.channel_layout) {
        ChannelConverter channel_converter;
        std::unique_ptr<AudioData> converted =
            channel_converter.convert(space_format.channel_layout, audio, p_error);
        if (p_error.has_error() || !converted) {
            LOWL_LOG_ERROR("Lowl::AudioSpace::add_audio: could not convert layout from " +
                           source_layout.to_string() + " to " + space_format.channel_layout.to_string() + ".");
            return InvalidAudioAssetHandle;
        }
        audio = std::move(converted);
    }

    std::lock_guard<std::mutex> lock(state_mutex);
    return insert_audio_asset_locked(std::move(audio));
}

Lowl::AudioAssetHandle Lowl::Audio::AudioSpace::add_audio(const std::string &p_path, Error &p_error) {
    std::unique_ptr<AudioData> audio_data = AudioReader::create_data(p_path, p_error);
    if (p_error.has_error()) {
        return InvalidAudioAssetHandle;
    }
    return add_audio(std::move(audio_data), p_error);
}

void Lowl::Audio::AudioSpace::remove_audio(const AudioAssetHandle p_audio_asset_handle) {
    std::lock_guard<std::mutex> lock(state_mutex);
    if (!p_audio_asset_handle.is_valid() || p_audio_asset_handle.owner_id != owner_id ||
        p_audio_asset_handle.id >= audio_asset_lookup.size()) {
        return;
    }

    AssetSlot &slot = audio_asset_lookup[p_audio_asset_handle.id];
    if (!slot.audio_data || slot.generation != p_audio_asset_handle.generation) {
        return;
    }
    recycle_audio_asset_locked(p_audio_asset_handle.id, slot);
}

Lowl::AudioPlaybackHandle Lowl::Audio::AudioSpace::create_playback(const AudioAssetHandle p_audio_asset_handle) {
    std::lock_guard<std::mutex> lock(state_mutex);
    drain_mixer_acks_locked();
    std::shared_ptr<AudioData> audio_data = get_audio_asset_locked(p_audio_asset_handle);
    if (!audio_data) {
        return InvalidAudioPlaybackHandle;
    }

    auto voice = std::make_unique<AudioVoice>(audio_data);
    voice->set_name(audio_data->get_name());
    return insert_playback_locked(std::move(voice), p_audio_asset_handle);
}

Lowl::AudioPlaybackHandle Lowl::Audio::AudioSpace::play_clip(const AudioAssetHandle p_audio_asset_handle) {
    const AudioPlaybackHandle playback_handle = create_playback(p_audio_asset_handle);
    if (playback_handle.is_valid()) {
        play(playback_handle);
    }
    return playback_handle;
}

void Lowl::Audio::AudioSpace::destroy_playback(const AudioPlaybackHandle p_audio_playback_handle) {
    std::lock_guard<std::mutex> lock(state_mutex);
    drain_mixer_acks_locked();
    if (!p_audio_playback_handle.is_valid() || p_audio_playback_handle.owner_id != owner_id ||
        p_audio_playback_handle.id >= playback_lookup.size()) {
        return;
    }

    PlaybackSlot &slot = playback_lookup[p_audio_playback_handle.id];
    if (!slot.voice || slot.slot_state == SlotState::Retiring ||
        slot.generation != p_audio_playback_handle.generation) {
        return;
    }
    retire_playback_locked(p_audio_playback_handle.id, slot);
}

void Lowl::Audio::AudioSpace::play(const AudioPlaybackHandle p_audio_playback_handle) {
    std::lock_guard<std::mutex> lock(state_mutex);
    drain_mixer_acks_locked();
    PlaybackSlot *slot = get_playback_slot_locked(p_audio_playback_handle);
    if (!slot) {
        return;
    }

    slot->voice->restart_playback();
    slot->mixer_submission_started = true;
    mixer.mix(slot->mixer_handle, slot->voice.get());
}

void Lowl::Audio::AudioSpace::pause(const AudioPlaybackHandle p_audio_playback_handle) {
    std::lock_guard<std::mutex> lock(state_mutex);
    PlaybackSlot *slot = get_playback_slot_locked(p_audio_playback_handle);
    if (!slot) {
        return;
    }
    slot->voice->pause_playback();
    if (slot->mixer_submission_started) {
        mixer.remove(slot->mixer_handle);
    }
}

void Lowl::Audio::AudioSpace::resume(const AudioPlaybackHandle p_audio_playback_handle) {
    std::lock_guard<std::mutex> lock(state_mutex);
    drain_mixer_acks_locked();
    PlaybackSlot *slot = get_playback_slot_locked(p_audio_playback_handle);
    if (!slot || slot->voice->get_playback_state() != AudioVoice::PlaybackState::Paused) {
        return;
    }
    slot->voice->resume_playback();
    slot->mixer_submission_started = true;
    mixer.mix(slot->mixer_handle, slot->voice.get());
}

void Lowl::Audio::AudioSpace::stop(const AudioPlaybackHandle p_audio_playback_handle) {
    std::lock_guard<std::mutex> lock(state_mutex);
    PlaybackSlot *slot = get_playback_slot_locked(p_audio_playback_handle);
    if (!slot) {
        return;
    }
    slot->voice->stop_playback();
    if (slot->mixer_submission_started) {
        mixer.remove(slot->mixer_handle);
    }
}

void Lowl::Audio::AudioSpace::clear_all_audio() {
    std::lock_guard<std::mutex> lock(state_mutex);
    drain_mixer_acks_locked();

    for (size_t index = FirstPlaybackSlotId; index < playback_lookup.size(); index++) {
        const AudioPlaybackId playback_id = static_cast<AudioPlaybackId>(index);
        PlaybackSlot &slot = playback_lookup[playback_id];
        if (slot.voice && slot.slot_state != SlotState::Retiring) {
            retire_playback_locked(playback_id, slot);
        }
    }
    for (size_t index = FirstAudioAssetId; index < audio_asset_lookup.size(); index++) {
        const AudioAssetId asset_id = static_cast<AudioAssetId>(index);
        AssetSlot &slot = audio_asset_lookup[asset_id];
        if (slot.audio_data) {
            recycle_audio_asset_locked(asset_id, slot);
        }
    }
}

void Lowl::Audio::AudioSpace::stop_all_audio() {
    std::lock_guard<std::mutex> lock(state_mutex);
    drain_mixer_acks_locked();

    for (size_t index = FirstPlaybackSlotId; index < playback_lookup.size(); index++) {
        const AudioPlaybackId playback_id = static_cast<AudioPlaybackId>(index);
        AudioPlaybackHandle handle{owner_id, playback_id, playback_lookup[playback_id].generation};
        PlaybackSlot *slot = get_playback_slot_locked(handle);
        if (!slot) {
            continue;
        }
        slot->voice->stop_playback();
        if (slot->mixer_submission_started) {
            mixer.remove(slot->mixer_handle);
        }
    }
}

void Lowl::Audio::AudioSpace::set_volume(const AudioPlaybackHandle p_audio_playback_handle,
                                         const Volume p_volume) {
    std::lock_guard<std::mutex> lock(state_mutex);
    PlaybackSlot *slot = get_playback_slot_locked(p_audio_playback_handle);
    if (slot) {
        slot->voice->set_volume(p_volume);
    }
}

void Lowl::Audio::AudioSpace::set_panning(const AudioPlaybackHandle p_audio_playback_handle,
                                          const Panning p_panning) {
    std::lock_guard<std::mutex> lock(state_mutex);
    PlaybackSlot *slot = get_playback_slot_locked(p_audio_playback_handle);
    if (slot) {
        slot->voice->set_panning(p_panning);
    }
}

void Lowl::Audio::AudioSpace::seek_frame(const AudioPlaybackHandle p_audio_playback_handle, const size_t p_frame) {
    std::lock_guard<std::mutex> lock(state_mutex);
    PlaybackSlot *slot = get_playback_slot_locked(p_audio_playback_handle);
    if (slot) {
        slot->voice->seek_frame(p_frame);
    }
}

void Lowl::Audio::AudioSpace::seek_time(const AudioPlaybackHandle p_audio_playback_handle,
                                        const double_l p_seconds) {
    std::lock_guard<std::mutex> lock(state_mutex);
    PlaybackSlot *slot = get_playback_slot_locked(p_audio_playback_handle);
    if (slot) {
        slot->voice->seek_time(p_seconds);
    }
}

void Lowl::Audio::AudioSpace::reset(const AudioPlaybackHandle p_audio_playback_handle) {
    std::lock_guard<std::mutex> lock(state_mutex);
    PlaybackSlot *slot = get_playback_slot_locked(p_audio_playback_handle);
    if (slot) {
        slot->voice->reset();
    }
}

Lowl::size_l
Lowl::Audio::AudioSpace::get_frame_position(const AudioPlaybackHandle p_audio_playback_handle) const {
    std::lock_guard<std::mutex> lock(state_mutex);
    const PlaybackSlot *slot = get_playback_slot_locked(p_audio_playback_handle);
    return slot ? slot->voice->get_frame_position() : 0;
}

Lowl::size_l
Lowl::Audio::AudioSpace::get_frames_remaining(const AudioPlaybackHandle p_audio_playback_handle) const {
    std::lock_guard<std::mutex> lock(state_mutex);
    const PlaybackSlot *slot = get_playback_slot_locked(p_audio_playback_handle);
    return slot ? slot->voice->get_frames_remaining() : 0;
}

Lowl::size_l Lowl::Audio::AudioSpace::get_frame_count(const AudioPlaybackHandle p_audio_playback_handle) const {
    std::lock_guard<std::mutex> lock(state_mutex);
    const PlaybackSlot *slot = get_playback_slot_locked(p_audio_playback_handle);
    return slot ? slot->voice->get_frame_count() : 0;
}

std::shared_ptr<Lowl::Audio::AudioData>
Lowl::Audio::AudioSpace::get_audio_asset_locked(const AudioAssetHandle p_audio_asset_handle) const {
    if (!p_audio_asset_handle.is_valid() || p_audio_asset_handle.owner_id != owner_id ||
        p_audio_asset_handle.id >= audio_asset_lookup.size()) {
        return nullptr;
    }
    const AssetSlot &slot = audio_asset_lookup[p_audio_asset_handle.id];
    if (!slot.audio_data || slot.generation != p_audio_asset_handle.generation) {
        return nullptr;
    }
    return slot.audio_data;
}

Lowl::Audio::AudioSpace::PlaybackSlot *
Lowl::Audio::AudioSpace::get_playback_slot_locked(const AudioPlaybackHandle p_audio_playback_handle) {
    if (!p_audio_playback_handle.is_valid() || p_audio_playback_handle.owner_id != owner_id ||
        p_audio_playback_handle.id >= playback_lookup.size()) {
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
    if (!p_audio_playback_handle.is_valid() || p_audio_playback_handle.owner_id != owner_id ||
        p_audio_playback_handle.id >= playback_lookup.size()) {
        return nullptr;
    }
    const PlaybackSlot &slot = playback_lookup[p_audio_playback_handle.id];
    if (!slot.voice || slot.slot_state == SlotState::Retiring ||
        slot.generation != p_audio_playback_handle.generation) {
        return nullptr;
    }
    return &slot;
}

Lowl::Audio::AudioSource::RenderResult
Lowl::Audio::AudioSpace::mix_into(AudioBlockView p_block, const MixGainVector &p_upstream_gain) {
    if (!playback_enabled.load(std::memory_order_relaxed)) {
        return {0, RenderState::Starved};
    }
    return mixer.mix_into(p_block, compose_gain_vector(p_upstream_gain));
}

Lowl::size_l Lowl::Audio::AudioSpace::get_frames_remaining() const {
    return LiveFrameCountSentinel;
}

Lowl::size_l Lowl::Audio::AudioSpace::get_frame_position() const {
    return 0;
}

Lowl::size_l Lowl::Audio::AudioSpace::get_frame_count() const {
    return LiveFrameCountSentinel;
}

std::map<Lowl::AudioAssetId, std::string> Lowl::Audio::AudioSpace::get_name_mapping() const {
    std::lock_guard<std::mutex> lock(state_mutex);
    std::map<AudioAssetId, std::string> mapping;
    for (size_t index = FirstAudioAssetId; index < audio_asset_lookup.size(); index++) {
        const AudioAssetId asset_id = static_cast<AudioAssetId>(index);
        const AssetSlot &slot = audio_asset_lookup[asset_id];
        if (slot.audio_data) {
            mapping.insert_or_assign(asset_id, slot.audio_data->get_name());
        }
    }
    return mapping;
}
