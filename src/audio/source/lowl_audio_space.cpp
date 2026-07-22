#include "lowl_audio_space.h"

#include <algorithm>
#include <atomic>
#include <cstdlib>
#include <limits>
#include <utility>

#include "audio/convert/lowl_audio_channel_converter.h"
#include "audio/convert/lowl_audio_re_sampler_r8b.h"
#include "audio/lowl_audio_utilities.h"
#include "audio/reader/lowl_audio_reader.h"
#include "lowl_logger.h"

namespace {
    std::atomic<Lowl::AudioInstanceId> next_audio_space_owner_id{1};

    template <typename T>
    T advance_allocation_id(const T p_current) {
        return p_current == std::numeric_limits<T>::max() ? static_cast<T>(0) : static_cast<T>(p_current + 1);
    }

    bool advance_generation(Lowl::AudioGeneration &p_generation) {
        if (p_generation == std::numeric_limits<Lowl::AudioGeneration>::max()) {
            p_generation = 0;
            return false;
        }
        p_generation++;
        return true;
    }

    Lowl::AudioInstanceId allocate_audio_space_owner_id() {
        const Lowl::AudioInstanceId owner_id = next_audio_space_owner_id.fetch_add(1, std::memory_order_relaxed);
        if (owner_id == 0 || owner_id == std::numeric_limits<Lowl::AudioInstanceId>::max()) {
            LOWL_LOG_ERROR("AudioSpace: process-wide owner identity capacity is exhausted.");
            std::abort();
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
    return {owner_id, asset_id, slot.generation};
}

Lowl::AudioPlaybackHandle
Lowl::Audio::AudioSpace::insert_playback_locked(std::unique_ptr<AudioVoice> p_voice,
                                                const AudioAssetHandle p_audio_asset_handle) {
    if (!p_voice) {
        return InvalidAudioPlaybackHandle;
    }

    AudioPlaybackId playback_id = InvalidPlaybackSlotId;
    const bool reusing_slot = !free_playback_slots.empty();
    if (reusing_slot) {
        playback_id = free_playback_slots.back();
    } else {
        if (current_audio_playback_slot_id == InvalidPlaybackSlotId) {
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
        return InvalidAudioPlaybackHandle;
    }
    if (reusing_slot) {
        free_playback_slots.pop_back();
    } else {
        current_audio_playback_slot_id = advance_allocation_id(playback_id);
    }

    PlaybackSlot &slot = playback_lookup[playback_id];
    slot.voice = std::move(p_voice);
    slot.audio_asset_handle = p_audio_asset_handle;
    slot.mixer_handle = {};
    slot.slot_state = SlotState::Active;
    return {owner_id, playback_id, slot.generation};
}

void Lowl::Audio::AudioSpace::recycle_audio_asset_locked(const AudioAssetId p_asset_id, AssetSlot &p_slot) {
    p_slot.audio_data.reset();
    if (!advance_generation(p_slot.generation)) {
        LOWL_LOG_ERROR("AudioSpace::recycle_audio_asset_locked: asset generation exhausted; slot retired.");
        return;
    }
    if (p_asset_id != InvalidAudioAssetId) {
        free_audio_asset_slots.push_back(p_asset_id);
    }
}

void Lowl::Audio::AudioSpace::recycle_playback_locked(const AudioPlaybackId p_slot_id, PlaybackSlot &p_slot) {
    clear_mixer_connection_locked(p_slot_id, p_slot);

    p_slot.voice.reset();
    p_slot.audio_asset_handle = InvalidAudioAssetHandle;
    p_slot.slot_state = SlotState::Active;
    if (!advance_generation(p_slot.generation)) {
        LOWL_LOG_ERROR("AudioSpace::recycle_playback_locked: playback generation exhausted; slot retired.");
        return;
    }
    if (p_slot_id != InvalidPlaybackSlotId) {
        free_playback_slots.push_back(p_slot_id);
    }
}

bool Lowl::Audio::AudioSpace::connect_playback_locked(const AudioPlaybackId p_slot_id,
                                                      PlaybackSlot &p_slot,
                                                      Error &p_error) {
    if (!p_slot.voice) {
        LOWL_LOG_ERROR("AudioSpace::connect_playback_locked: playback has no voice.");
        p_error.set_error(ErrorCode::AudioPlaybackHandleInvalid);
        return false;
    }
    if (p_slot.mixer_handle.is_valid()) {
        return true;
    }

    const AudioMixerHandle mixer_handle = mixer.connect(*p_slot.voice, p_error);
    if (p_error.has_error() || !mixer_handle.is_valid()) {
        LOWL_LOG_ERROR("AudioSpace::connect_playback_locked: mixer rejected the connection.");
        return false;
    }

    p_slot.mixer_handle = mixer_handle;
    mixer_playback_lookup[mixer_handle.connection_id] = p_slot_id;
    return true;
}

void Lowl::Audio::AudioSpace::clear_mixer_connection_locked(const AudioPlaybackId p_slot_id,
                                                             PlaybackSlot &p_slot) {
    if (!p_slot.mixer_handle.is_valid()) {
        return;
    }

    const AudioMixerConnectionId mixer_connection_id = p_slot.mixer_handle.connection_id;
    if (mixer_connection_id < mixer_playback_lookup.size() &&
        mixer_playback_lookup[mixer_connection_id] == p_slot_id) {
        mixer_playback_lookup[mixer_connection_id] = InvalidPlaybackSlotId;
    }
    p_slot.mixer_handle = {};
}

bool Lowl::Audio::AudioSpace::retire_playback_locked(const AudioPlaybackId p_slot_id,
                                                     PlaybackSlot &p_slot,
                                                     Error &p_error) {
    if (!p_slot.voice || p_slot.slot_state == SlotState::Retiring) {
        LOWL_LOG_ERROR("AudioSpace::retire_playback_locked: playback is invalid or already retiring.");
        p_error.set_error(ErrorCode::AudioPlaybackHandleInvalid);
        return false;
    }

    if (!p_slot.mixer_handle.is_valid()) {
        recycle_playback_locked(p_slot_id, p_slot);
        return true;
    }

    p_slot.voice->stop_playback();
    mixer.disconnect(p_slot.mixer_handle, p_error);
    if (p_error.has_error()) {
        LOWL_LOG_ERROR("AudioSpace::retire_playback_locked: mixer rejected the disconnection request.");
        return false;
    }
    p_slot.slot_state = SlotState::Retiring;
    return true;
}

Lowl::AudioPlaybackId
Lowl::Audio::AudioSpace::find_playback_slot_id_by_mixer_handle_locked(const AudioMixerHandle p_mixer_handle) const {
    if (!p_mixer_handle.is_valid() || p_mixer_handle.connection_id >= mixer_playback_lookup.size()) {
        return InvalidPlaybackSlotId;
    }
    const AudioPlaybackId playback_id = mixer_playback_lookup[p_mixer_handle.connection_id];
    if (playback_id == InvalidPlaybackSlotId || playback_id >= playback_lookup.size()) {
        return InvalidPlaybackSlotId;
    }
    const PlaybackSlot &slot = playback_lookup[playback_id];
    return slot.voice && slot.mixer_handle == p_mixer_handle ? playback_id : InvalidPlaybackSlotId;
}

void Lowl::Audio::AudioSpace::collect_mixer_completions_locked() {
    AudioMixerCompletion completion{};
    for (size_t completion_count = 0;
         completion_count < AudioMixer::MaxConnections && mixer.try_collect_completion(completion);
         completion_count++) {
        const AudioPlaybackId playback_id = find_playback_slot_id_by_mixer_handle_locked(completion.handle);
        if (playback_id == InvalidPlaybackSlotId) {
            continue;
        }

        PlaybackSlot &slot = playback_lookup[playback_id];
        if (completion.type == AudioMixerCompletion::Type::Finished) {
            slot.voice->stop_playback();
        }

        if (slot.slot_state == SlotState::Retiring) {
            recycle_playback_locked(playback_id, slot);
            continue;
        }
        clear_mixer_connection_locked(playback_id, slot);
    }
}

Lowl::AudioAssetHandle Lowl::Audio::AudioSpace::add_audio(std::unique_ptr<AudioData> p_audio_data, Error &p_error) {
    p_error.clear();

    std::shared_ptr<AudioData> audio = std::move(p_audio_data);
    if (!audio) {
        LOWL_LOG_ERROR("AudioSpace::add_audio: audio data must not be null.");
        p_error.set_error(ErrorCode::InvalidParameter);
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
    const AudioAssetHandle handle = insert_audio_asset_locked(std::move(audio));
    if (!handle.is_valid()) {
        LOWL_LOG_ERROR("AudioSpace::add_audio: fixed asset handle capacity is exhausted.");
        p_error.set_error(ErrorCode::AudioAssetCapacityExhausted);
    }
    return handle;
}

Lowl::AudioAssetHandle Lowl::Audio::AudioSpace::add_audio(const std::string &p_path, Error &p_error) {
    p_error.clear();
    std::unique_ptr<AudioData> audio_data = AudioReader::create_data(p_path, p_error);
    if (p_error.has_error()) {
        return InvalidAudioAssetHandle;
    }
    return add_audio(std::move(audio_data), p_error);
}

void Lowl::Audio::AudioSpace::remove_audio(const AudioAssetHandle p_audio_asset_handle, Error &p_error) {
    p_error.clear();
    std::lock_guard<std::mutex> lock(state_mutex);
    if (!p_audio_asset_handle.is_valid() || p_audio_asset_handle.owner_id != owner_id ||
        p_audio_asset_handle.id >= audio_asset_lookup.size()) {
        LOWL_LOG_ERROR("AudioSpace::remove_audio: asset handle is invalid or belongs to another space.");
        p_error.set_error(ErrorCode::AudioAssetHandleInvalid);
        return;
    }

    AssetSlot &slot = audio_asset_lookup[p_audio_asset_handle.id];
    if (!slot.audio_data || slot.generation != p_audio_asset_handle.generation) {
        LOWL_LOG_ERROR("AudioSpace::remove_audio: asset handle is stale or already removed.");
        p_error.set_error(ErrorCode::AudioAssetHandleInvalid);
        return;
    }
    recycle_audio_asset_locked(p_audio_asset_handle.id, slot);
}

Lowl::AudioPlaybackHandle Lowl::Audio::AudioSpace::create_playback(const AudioAssetHandle p_audio_asset_handle,
                                                                   Error &p_error) {
    p_error.clear();
    std::lock_guard<std::mutex> lock(state_mutex);
    collect_mixer_completions_locked();
    std::shared_ptr<AudioData> audio_data = get_audio_asset_locked(p_audio_asset_handle);
    if (!audio_data) {
        LOWL_LOG_ERROR("AudioSpace::create_playback: asset handle is invalid, stale, or belongs to another space.");
        p_error.set_error(ErrorCode::AudioAssetHandleInvalid);
        return InvalidAudioPlaybackHandle;
    }

    auto voice = std::make_unique<AudioVoice>(audio_data);
    voice->set_name(audio_data->get_name());
    const AudioPlaybackHandle handle = insert_playback_locked(std::move(voice), p_audio_asset_handle);
    if (!handle.is_valid()) {
        LOWL_LOG_ERROR("AudioSpace::create_playback: fixed playback handle capacity is exhausted.");
        p_error.set_error(ErrorCode::AudioPlaybackCapacityExhausted);
    }
    return handle;
}

Lowl::AudioPlaybackHandle Lowl::Audio::AudioSpace::play_asset(const AudioAssetHandle p_audio_asset_handle,
                                                              Error &p_error) {
    p_error.clear();
    const AudioPlaybackHandle playback_handle = create_playback(p_audio_asset_handle, p_error);
    if (p_error.has_error() || !playback_handle.is_valid()) {
        return InvalidAudioPlaybackHandle;
    }

    play(playback_handle, p_error);
    if (p_error.has_error()) {
        Error cleanup_error;
        destroy_playback(playback_handle, cleanup_error);
        return InvalidAudioPlaybackHandle;
    }
    return playback_handle;
}

void Lowl::Audio::AudioSpace::destroy_playback(const AudioPlaybackHandle p_audio_playback_handle, Error &p_error) {
    p_error.clear();
    std::lock_guard<std::mutex> lock(state_mutex);
    collect_mixer_completions_locked();
    PlaybackSlot *slot = require_playback_slot_locked(
        p_audio_playback_handle,
        p_error,
        "AudioSpace::destroy_playback: playback handle is invalid, stale, or retiring.");
    if (!slot) {
        return;
    }
    retire_playback_locked(p_audio_playback_handle.id, *slot, p_error);
}

void Lowl::Audio::AudioSpace::play(const AudioPlaybackHandle p_audio_playback_handle, Error &p_error) {
    p_error.clear();
    std::lock_guard<std::mutex> lock(state_mutex);
    collect_mixer_completions_locked();
    PlaybackSlot *slot = require_playback_slot_locked(
        p_audio_playback_handle,
        p_error,
        "AudioSpace::play: playback handle is invalid, stale, or retiring.");
    if (!slot) {
        return;
    }
    if (!connect_playback_locked(p_audio_playback_handle.id, *slot, p_error)) {
        return;
    }

    slot->voice->restart_playback();
}

void Lowl::Audio::AudioSpace::pause(const AudioPlaybackHandle p_audio_playback_handle, Error &p_error) {
    p_error.clear();
    std::lock_guard<std::mutex> lock(state_mutex);
    PlaybackSlot *slot = require_playback_slot_locked(
        p_audio_playback_handle,
        p_error,
        "AudioSpace::pause: playback handle is invalid, stale, or retiring.");
    if (!slot) {
        return;
    }
    slot->voice->pause_playback();
}

void Lowl::Audio::AudioSpace::resume(const AudioPlaybackHandle p_audio_playback_handle, Error &p_error) {
    p_error.clear();
    std::lock_guard<std::mutex> lock(state_mutex);
    collect_mixer_completions_locked();
    PlaybackSlot *slot = require_playback_slot_locked(
        p_audio_playback_handle,
        p_error,
        "AudioSpace::resume: playback handle is invalid, stale, or retiring.");
    if (!slot) {
        return;
    }
    if (slot->voice->get_playback_state() != AudioVoice::PlaybackState::Paused) {
        LOWL_LOG_ERROR("AudioSpace::resume: playback is not paused.");
        p_error.set_error(ErrorCode::InvalidOperationWhileActive);
        return;
    }
    slot->voice->resume_playback();
}

void Lowl::Audio::AudioSpace::stop(const AudioPlaybackHandle p_audio_playback_handle, Error &p_error) {
    p_error.clear();
    std::lock_guard<std::mutex> lock(state_mutex);
    PlaybackSlot *slot = require_playback_slot_locked(
        p_audio_playback_handle,
        p_error,
        "AudioSpace::stop: playback handle is invalid, stale, or retiring.");
    if (!slot) {
        return;
    }
    slot->voice->stop_playback();
}

void Lowl::Audio::AudioSpace::clear_all_audio(Error &p_error) {
    p_error.clear();
    std::lock_guard<std::mutex> lock(state_mutex);
    collect_mixer_completions_locked();

    for (size_t index = FirstPlaybackSlotId; index < playback_lookup.size(); index++) {
        const AudioPlaybackId playback_id = static_cast<AudioPlaybackId>(index);
        PlaybackSlot &slot = playback_lookup[playback_id];
        if (slot.voice && slot.slot_state != SlotState::Retiring) {
            if (!retire_playback_locked(playback_id, slot, p_error)) {
                return;
            }
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
    collect_mixer_completions_locked();

    for (size_t index = FirstPlaybackSlotId; index < playback_lookup.size(); index++) {
        const AudioPlaybackId playback_id = static_cast<AudioPlaybackId>(index);
        AudioPlaybackHandle handle{owner_id, playback_id, playback_lookup[playback_id].generation};
        PlaybackSlot *slot = get_playback_slot_locked(handle);
        if (!slot) {
            continue;
        }
        slot->voice->stop_playback();
    }
}

void Lowl::Audio::AudioSpace::set_volume(const AudioPlaybackHandle p_audio_playback_handle,
                                         const Volume p_volume,
                                         Error &p_error) {
    p_error.clear();
    std::lock_guard<std::mutex> lock(state_mutex);
    PlaybackSlot *slot = require_playback_slot_locked(
        p_audio_playback_handle,
        p_error,
        "AudioSpace::set_volume: playback handle is invalid, stale, or retiring.");
    if (!slot) {
        return;
    }
    slot->voice->set_volume(p_volume);
}

void Lowl::Audio::AudioSpace::set_panning(const AudioPlaybackHandle p_audio_playback_handle,
                                          const Panning p_panning,
                                          Error &p_error) {
    p_error.clear();
    std::lock_guard<std::mutex> lock(state_mutex);
    PlaybackSlot *slot = require_playback_slot_locked(
        p_audio_playback_handle,
        p_error,
        "AudioSpace::set_panning: playback handle is invalid, stale, or retiring.");
    if (!slot) {
        return;
    }
    slot->voice->set_panning(p_panning);
}

void Lowl::Audio::AudioSpace::seek_frame(const AudioPlaybackHandle p_audio_playback_handle,
                                        const size_t p_frame,
                                        Error &p_error) {
    p_error.clear();
    std::lock_guard<std::mutex> lock(state_mutex);
    PlaybackSlot *slot = require_playback_slot_locked(
        p_audio_playback_handle,
        p_error,
        "AudioSpace::seek_frame: playback handle is invalid, stale, or retiring.");
    if (!slot) {
        return;
    }
    slot->voice->seek_frame(p_frame);
}

void Lowl::Audio::AudioSpace::seek_time(const AudioPlaybackHandle p_audio_playback_handle,
                                        const double_l p_seconds,
                                        Error &p_error) {
    p_error.clear();
    std::lock_guard<std::mutex> lock(state_mutex);
    PlaybackSlot *slot = require_playback_slot_locked(
        p_audio_playback_handle,
        p_error,
        "AudioSpace::seek_time: playback handle is invalid, stale, or retiring.");
    if (!slot) {
        return;
    }
    slot->voice->seek_time(p_seconds);
}

void Lowl::Audio::AudioSpace::reset(const AudioPlaybackHandle p_audio_playback_handle, Error &p_error) {
    p_error.clear();
    std::lock_guard<std::mutex> lock(state_mutex);
    PlaybackSlot *slot = require_playback_slot_locked(
        p_audio_playback_handle,
        p_error,
        "AudioSpace::reset: playback handle is invalid, stale, or retiring.");
    if (!slot) {
        return;
    }
    slot->voice->reset();
}

Lowl::size_l
Lowl::Audio::AudioSpace::get_frame_position(const AudioPlaybackHandle p_audio_playback_handle,
                                            Error &p_error) const {
    p_error.clear();
    std::lock_guard<std::mutex> lock(state_mutex);
    const PlaybackSlot *slot = require_playback_slot_locked(
        p_audio_playback_handle,
        p_error,
        "AudioSpace::get_frame_position: playback handle is invalid, stale, or retiring.");
    if (!slot) {
        return 0;
    }
    return slot->voice->get_frame_position();
}

Lowl::size_l
Lowl::Audio::AudioSpace::get_frames_remaining(const AudioPlaybackHandle p_audio_playback_handle,
                                              Error &p_error) const {
    p_error.clear();
    std::lock_guard<std::mutex> lock(state_mutex);
    const PlaybackSlot *slot = require_playback_slot_locked(
        p_audio_playback_handle,
        p_error,
        "AudioSpace::get_frames_remaining: playback handle is invalid, stale, or retiring.");
    if (!slot) {
        return 0;
    }
    return slot->voice->get_frames_remaining();
}

Lowl::size_l Lowl::Audio::AudioSpace::get_frame_count(const AudioPlaybackHandle p_audio_playback_handle,
                                                      Error &p_error) const {
    p_error.clear();
    std::lock_guard<std::mutex> lock(state_mutex);
    const PlaybackSlot *slot = require_playback_slot_locked(
        p_audio_playback_handle,
        p_error,
        "AudioSpace::get_frame_count: playback handle is invalid, stale, or retiring.");
    if (!slot) {
        return 0;
    }
    return slot->voice->get_frame_count();
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

Lowl::Audio::AudioSpace::PlaybackSlot *
Lowl::Audio::AudioSpace::require_playback_slot_locked(const AudioPlaybackHandle p_audio_playback_handle,
                                                      Error &p_error,
                                                      const char *p_error_message) {
    PlaybackSlot *slot = get_playback_slot_locked(p_audio_playback_handle);
    if (slot) {
        return slot;
    }

    LOWL_LOG_ERROR(p_error_message);
    p_error.set_error(ErrorCode::AudioPlaybackHandleInvalid);
    return nullptr;
}

const Lowl::Audio::AudioSpace::PlaybackSlot *
Lowl::Audio::AudioSpace::require_playback_slot_locked(const AudioPlaybackHandle p_audio_playback_handle,
                                                      Error &p_error,
                                                      const char *p_error_message) const {
    const PlaybackSlot *slot = get_playback_slot_locked(p_audio_playback_handle);
    if (slot) {
        return slot;
    }

    LOWL_LOG_ERROR(p_error_message);
    p_error.set_error(ErrorCode::AudioPlaybackHandleInvalid);
    return nullptr;
}

Lowl::Audio::AudioSource::RenderResult
Lowl::Audio::AudioSpace::mix_into(AudioBlockView p_block, const MixGainVector &p_upstream_gain) {
    if (!playback_enabled.load(std::memory_order_relaxed)) {
        // Aggregate pause suppresses audio, but the private mixer must still consume
        // connection and disconnection lifecycle work from the render thread.
        AudioBlockView control_block{};
        control_block.channel_count = get_channel_count();
        mixer.mix_into(control_block, make_unity_gain_vector());
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
