#include "lowl_audio_space.h"

#include <algorithm>
#include <limits>

#include "./lowl_logger.h"
#include "audio/convert/lowl_audio_channel_converter.h"
#include "audio/convert/lowl_audio_re_sampler_r8b.h"
#include "audio/lowl_audio_utilities.h"
#include "audio/reader/lowl_audio_reader.h"

namespace {
    std::atomic<Lowl::uint32_l> next_audio_space_owner_id{1};

    template <typename T>
    T advance_id(const T p_current) {
        return p_current == std::numeric_limits<T>::max() ? 1 : static_cast<T>(p_current + 1);
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

Lowl::Audio::AudioSpace::AudioSpace(const SampleRate p_sample_rate,
                                    const ChannelLayout p_channel_layout,
                                    const uint32_t p_mixer_scratch_buffer_capacity)
    : AudioSource(p_sample_rate, p_channel_layout) {
    mixer = std::make_unique<AudioMixer>(sample_rate, channel_layout, p_mixer_scratch_buffer_capacity);
    owner_id = allocate_audio_space_owner_id();
    mixer_owner_id = mixer->register_ack_owner();
    current_audio_asset_id = FirstAudioAssetId;
    current_audio_playback_slot_id = FirstPlaybackSlotId;
    audio_asset_lookup = std::vector<AssetSlot>();
    playback_lookup = std::vector<PlaybackSlot>();
    mixer_handle_lookup = std::vector<AudioPlaybackId>();
    free_audio_asset_slots = std::vector<AudioAssetId>();
    free_playback_slots = std::vector<AudioPlaybackId>();
}

Lowl::Audio::AudioSpace::~AudioSpace() {
    std::lock_guard<std::mutex> lock(state_mutex);
    for (PlaybackSlot &slot : playback_lookup) {
        if (!slot.mixer_handle.is_valid()) {
            continue;
        }
        mixer->release_handle(slot.mixer_handle);
        slot.mixer_handle = {};
    }
    if (mixer && mixer_owner_id != 0) {
        mixer->unregister_ack_owner(mixer_owner_id);
    }
}

Lowl::AudioAssetHandle Lowl::Audio::AudioSpace::insert_audio_asset_locked(std::shared_ptr<AudioData> p_audio_data) {
    AudioAssetId asset_id = InvalidAudioAssetId;
    if (!free_audio_asset_slots.empty()) {
        asset_id = free_audio_asset_slots.back();
        free_audio_asset_slots.pop_back();
    } else {
        if (current_audio_asset_id == InvalidAudioAssetId) {
            return InvalidAudioAssetHandle;
        }
        asset_id = current_audio_asset_id;
        const size_t required_size = static_cast<size_t>(asset_id) + 1;
        if (audio_asset_lookup.size() < required_size) {
            audio_asset_lookup.resize(required_size + LookupGrowth);
        }
        current_audio_asset_id = advance_id(asset_id);
    }

    if (asset_id == InvalidAudioAssetId || asset_id >= audio_asset_lookup.size()) {
        return InvalidAudioAssetHandle;
    }
    AssetSlot &slot = audio_asset_lookup[asset_id];
    slot.audio_data = std::move(p_audio_data);
    if (slot.generation == 0) {
        slot.generation = 1;
    }

    AudioAssetHandle handle{};
    handle.owner_id = owner_id;
    handle.id = asset_id;
    handle.generation = slot.generation;
    return handle;
}

Lowl::AudioPlaybackHandle Lowl::Audio::AudioSpace::insert_playback_locked(std::unique_ptr<AudioVoice> p_voice,
                                                                          const AudioAssetHandle p_audio_asset_handle) {
    if (!p_voice) {
        return InvalidAudioPlaybackHandle;
    }

    const AudioMixerHandle mixer_handle = mixer->allocate_handle(mixer_owner_id);
    if (!mixer_handle.is_valid()) {
        return InvalidAudioPlaybackHandle;
    }

    AudioPlaybackId slot_id = InvalidPlaybackSlotId;
    const bool reusing_slot = !free_playback_slots.empty();
    if (!free_playback_slots.empty()) {
        slot_id = free_playback_slots.back();
    } else {
        if (current_audio_playback_slot_id == InvalidPlaybackSlotId) {
            mixer->release_handle(mixer_handle);
            return InvalidAudioPlaybackHandle;
        }
        slot_id = current_audio_playback_slot_id;
        const size_t required_size = static_cast<size_t>(slot_id) + 1;
        if (playback_lookup.size() < required_size) {
            playback_lookup.resize(required_size + LookupGrowth);
        }
    }

    if (slot_id == InvalidPlaybackSlotId || slot_id >= playback_lookup.size()) {
        mixer->release_handle(mixer_handle);
        return InvalidAudioPlaybackHandle;
    }
    if (reusing_slot) {
        free_playback_slots.pop_back();
    } else {
        current_audio_playback_slot_id = advance_id(slot_id);
    }
    const size_t required_mixer_lookup_size = static_cast<size_t>(mixer_handle.playback_id) + 1;
    if (mixer_handle_lookup.size() < required_mixer_lookup_size) {
        mixer_handle_lookup.resize(required_mixer_lookup_size, InvalidPlaybackSlotId);
    }
    PlaybackSlot &slot = playback_lookup[slot_id];
    slot.voice = std::move(p_voice);
    slot.audio_asset_handle = p_audio_asset_handle;
    slot.mixer_handle = mixer_handle;
    if (slot.generation == 0) {
        slot.generation = 1;
    }
    slot.slot_state = SlotState::Active;
    slot.voice->stop_playback();
    mixer_handle_lookup[mixer_handle.playback_id] = slot_id;

    AudioPlaybackHandle handle{};
    handle.owner_id = owner_id;
    handle.id = slot_id;
    handle.generation = slot.generation;
    return handle;
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
        if (p_slot.mixer_handle.playback_id < mixer_handle_lookup.size() &&
            mixer_handle_lookup[p_slot.mixer_handle.playback_id] == p_slot_id) {
            mixer_handle_lookup[p_slot.mixer_handle.playback_id] = InvalidPlaybackSlotId;
        }
        mixer->release_handle(p_slot.mixer_handle);
        p_slot.mixer_handle = {};
    }
    p_slot.voice.reset();
    p_slot.audio_asset_handle = InvalidAudioAssetHandle;
    p_slot.slot_state = SlotState::Active;
    p_slot.generation = advance_generation(p_slot.generation);
    if (p_slot_id != InvalidPlaybackSlotId) {
        free_playback_slots.push_back(p_slot_id);
    }
}

void Lowl::Audio::AudioSpace::drain_mixer_acks_locked() {
    AudioMixerAck ack = {};
    while (mixer->try_dequeue_ack(mixer_owner_id, ack)) {
        if (!ack.handle.is_valid() || ack.handle.owner_id != mixer_owner_id) {
            continue;
        }
        const AudioPlaybackId playback_id = find_playback_slot_id_by_mixer_handle_locked(ack.handle);
        if (playback_id == InvalidPlaybackSlotId || playback_id >= playback_lookup.size()) {
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
                    recycle_playback_locked(playback_id, slot);
                } else if (slot.voice->get_playback_state() == AudioVoice::PlaybackState::Playing) {
                    slot.voice->pause_playback();
                }
                break;
            }
        }
    }
}

Lowl::AudioAssetHandle Lowl::Audio::AudioSpace::add_audio(std::unique_ptr<AudioData> p_audio_data, Error &error) {
    std::shared_ptr<AudioData> audio = std::move(p_audio_data);
    if (!audio) {
        return InvalidAudioAssetHandle;
    }
    SampleRate rate = audio->get_sample_rate();

    if (!Lowl::Audio::sample_rates_equal(rate, sample_rate)) {
        std::unique_ptr<AudioData> resampled = ReSamplerR8b::resample(audio, sample_rate);
        if (!resampled) {
            error.set_error(ErrorCode::Error);
            LOWL_LOG_ERROR("Lowl::Space::load resample failed.");
            return InvalidAudioAssetHandle;
        }
        audio = std::move(resampled);
    }

    ChannelLayout layout = audio->get_channel_layout();
    if (layout != channel_layout) {
        ChannelConverter channel_converter;
        std::unique_ptr<AudioData> converted = channel_converter.convert(channel_layout, audio, error);
        if (error.has_error()) {
            LOWL_LOG_ERROR("Lowl::Space::load channel_converter.convert() ErrCode:" +
                           std::to_string(error.get_error_code()) + " ErrText:" + error.get_error_text() +
                           ". Could not convert layout from " + layout.to_string() + " to " +
                           channel_layout.to_string() + ".");
            return InvalidAudioAssetHandle;
        }
        audio = std::move(converted);
    }

    std::lock_guard<std::mutex> lock(state_mutex);
    return insert_audio_asset_locked(std::move(audio));
}

Lowl::AudioAssetHandle Lowl::Audio::AudioSpace::add_audio(const std::string &p_path, Error &error) {
    std::unique_ptr<AudioData> audio_data = AudioReader::create_data(p_path, error);
    if (error.has_error()) {
        return InvalidAudioAssetHandle;
    }
    return add_audio(std::move(audio_data), error);
}

Lowl::AudioPlaybackHandle Lowl::Audio::AudioSpace::create_playback(const AudioAssetHandle p_audio_asset_handle) {
    std::lock_guard<std::mutex> lock(state_mutex);
    drain_mixer_acks_locked();
    std::shared_ptr<AudioData> audio_data = get_audio_asset_locked(p_audio_asset_handle);
    if (!audio_data) {
        return InvalidAudioPlaybackHandle;
    }
    std::unique_ptr<AudioVoice> voice = std::make_unique<AudioVoice>(audio_data);
    voice->set_name(audio_data->get_name());
    return insert_playback_locked(std::move(voice), p_audio_asset_handle);
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
        handle.owner_id = owner_id;
        handle.id = playback_slot_id;
        handle.generation = slot.generation;
        mixer->remove(get_mixer_handle_locked(handle), true);
    }

    for (AudioAssetId audio_asset_id = FirstAudioAssetId; audio_asset_id < audio_asset_lookup.size(); audio_asset_id++) {
        AssetSlot &slot = audio_asset_lookup[audio_asset_id];
        if (!slot.audio_data) {
            continue;
        }
        recycle_audio_asset_locked(audio_asset_id, slot);
    }
}

void Lowl::Audio::AudioSpace::stop_all_audio() {
    std::lock_guard<std::mutex> lock(state_mutex);
    drain_mixer_acks_locked();
    for (AudioPlaybackId playback_slot_id = FirstPlaybackSlotId; playback_slot_id < playback_lookup.size();
         playback_slot_id++) {
        AudioPlaybackHandle handle{};
        handle.owner_id = owner_id;
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

Lowl::AudioPlaybackId
Lowl::Audio::AudioSpace::find_playback_slot_id_by_mixer_handle_locked(const AudioMixerHandle p_mixer_handle) const {
    if (!p_mixer_handle.is_valid() || p_mixer_handle.playback_id >= mixer_handle_lookup.size()) {
        return InvalidPlaybackSlotId;
    }
    const AudioPlaybackId playback_slot_id = mixer_handle_lookup[p_mixer_handle.playback_id];
    if (playback_slot_id == InvalidPlaybackSlotId || playback_slot_id >= playback_lookup.size()) {
        return InvalidPlaybackSlotId;
    }
    const PlaybackSlot &slot = playback_lookup[playback_slot_id];
    return slot.mixer_handle == p_mixer_handle ? playback_slot_id : InvalidPlaybackSlotId;
}

Lowl::AudioMixerHandle
Lowl::Audio::AudioSpace::get_mixer_handle_locked(const AudioPlaybackHandle p_audio_playback_handle) const {
    if (!p_audio_playback_handle.is_valid() || p_audio_playback_handle.owner_id != owner_id ||
        p_audio_playback_handle.id >= playback_lookup.size()) {
        return {};
    }
    const PlaybackSlot &slot = playback_lookup[p_audio_playback_handle.id];
    if (slot.generation == 0 || slot.generation != p_audio_playback_handle.generation) {
        return {};
    }
    return slot.mixer_handle;
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

Lowl::Audio::AudioSource::RenderResult Lowl::Audio::AudioSpace::render(AudioBlockView p_block) {
    if (!playback_enabled.load(std::memory_order_relaxed)) {
        return {0, RenderState::Starved};
    }
    mixer->set_volume(get_volume());
    mixer->set_panning(get_panning());
    return mixer->render(p_block);
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
        const AssetSlot &slot = audio_asset_lookup[audio_asset_id];
        if (!slot.audio_data) {
            continue;
        }
        map.insert_or_assign(audio_asset_id, slot.audio_data->get_name());
    }
    return map;
}
