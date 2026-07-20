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
    : AudioSource(p_audio_format) {
    master_bus_node = std::make_unique<AudioBus>(get_audio_format());
    owner_id = allocate_audio_space_owner_id();
    current_audio_asset_id = FirstAudioAssetId;
    current_audio_bus_slot_id = FirstDynamicBusSlotId;
    current_audio_playback_slot_id = FirstPlaybackSlotId;
    current_audio_stream_slot_id = FirstStreamSlotId;
    audio_asset_lookup = std::vector<AssetSlot>();
    bus_lookup = std::vector<BusSlot>(static_cast<size_t>(FirstDynamicBusSlotId));
    playback_lookup = std::vector<PlaybackSlot>();
    stream_lookup = std::vector<StreamSlot>();
    root_playback_lookup = std::vector<AudioPlaybackId>();
    root_bus_lookup = std::vector<AudioBusId>();
    free_audio_asset_slots = std::vector<AudioAssetId>();
    free_bus_slots = std::vector<AudioBusId>();
    free_playback_slots = std::vector<AudioPlaybackId>();
    free_stream_slots = std::vector<AudioStreamId>();
}

Lowl::Audio::AudioSpace::~AudioSpace() {
    std::lock_guard<std::mutex> lock(state_mutex);
    for (BusSlot &slot : bus_lookup) {
        if (!slot.bus) {
            continue;
        }
        if (slot.bus_slot_handle.is_valid()) {
            AudioBus *parent_bus = get_bus_node_locked(slot.parent_bus_handle);
            if (parent_bus != nullptr) {
                parent_bus->release_handle(slot.bus_slot_handle);
            }
            slot.bus_slot_handle = {};
        }
    }
    for (PlaybackSlot &slot : playback_lookup) {
        if (!slot.bus_slot_handle.is_valid()) {
            continue;
        }
        AudioBus *bus = get_bus_node_locked(slot.audio_bus_handle);
        if (bus != nullptr) {
            bus->release_handle(slot.bus_slot_handle);
        }
        slot.bus_slot_handle = {};
    }
    for (StreamSlot &slot : stream_lookup) {
        if (!slot.bus_slot_handle.is_valid()) {
            continue;
        }
        AudioBus *bus = get_bus_node_locked(slot.audio_bus_handle);
        if (bus != nullptr) {
            bus->release_handle(slot.bus_slot_handle);
        }
        slot.bus_slot_handle = {};
    }
}

Lowl::AudioBusHandle Lowl::Audio::AudioSpace::get_master_bus_handle() const {
    AudioBusHandle handle{};
    handle.owner_id = owner_id;
    handle.id = MasterBusSlotId;
    handle.generation = 1;
    return handle;
}

Lowl::AudioBusHandle Lowl::Audio::AudioSpace::normalize_bus_handle_locked(const AudioBusHandle p_audio_bus_handle) const {
    if (!p_audio_bus_handle.is_valid()) {
        return get_master_bus_handle();
    }
    return p_audio_bus_handle;
}

Lowl::Audio::AudioBus *Lowl::Audio::AudioSpace::get_bus_node_locked(const AudioBusHandle p_audio_bus_handle) {
    const AudioBusHandle normalized = normalize_bus_handle_locked(p_audio_bus_handle);
    if (normalized.owner_id != owner_id) {
        return nullptr;
    }
    if (normalized.id == MasterBusSlotId) {
        return master_bus_node.get();
    }
    BusSlot *slot = get_bus_slot_locked(normalized);
    return slot ? slot->bus.get() : nullptr;
}

const Lowl::Audio::AudioBus *Lowl::Audio::AudioSpace::get_bus_node_locked(const AudioBusHandle p_audio_bus_handle) const {
    const AudioBusHandle normalized = normalize_bus_handle_locked(p_audio_bus_handle);
    if (normalized.owner_id != owner_id) {
        return nullptr;
    }
    if (normalized.id == MasterBusSlotId) {
        return master_bus_node.get();
    }
    const BusSlot *slot = get_bus_slot_locked(normalized);
    return slot ? slot->bus.get() : nullptr;
}

void Lowl::Audio::AudioSpace::map_playback_handle_locked(const AudioBusHandle p_audio_bus_handle,
                                                         const AudioBusSlotHandle p_bus_slot_handle,
                                                         const AudioPlaybackId p_playback_slot_id) {
    if (!p_bus_slot_handle.is_valid() || p_playback_slot_id == InvalidPlaybackSlotId) {
        return;
    }

    const size_t required_size = static_cast<size_t>(p_bus_slot_handle.slot_id) + 1;
    const AudioBusHandle normalized = normalize_bus_handle_locked(p_audio_bus_handle);
    if (normalized.id == MasterBusSlotId) {
        if (root_playback_lookup.size() < required_size) {
            root_playback_lookup.resize(required_size, InvalidPlaybackSlotId);
        }
        root_playback_lookup[p_bus_slot_handle.slot_id] = p_playback_slot_id;
        return;
    }

    BusSlot *slot = get_bus_slot_locked(normalized);
    if (slot == nullptr) {
        return;
    }
    if (slot->playback_lookup.size() < required_size) {
        slot->playback_lookup.resize(required_size, InvalidPlaybackSlotId);
    }
    slot->playback_lookup[p_bus_slot_handle.slot_id] = p_playback_slot_id;
}

void Lowl::Audio::AudioSpace::unmap_playback_handle_locked(const AudioBusHandle p_audio_bus_handle,
                                                           const AudioBusSlotHandle p_bus_slot_handle) {
    if (!p_bus_slot_handle.is_valid()) {
        return;
    }

    const AudioBusHandle normalized = normalize_bus_handle_locked(p_audio_bus_handle);
    if (normalized.id == MasterBusSlotId) {
        if (p_bus_slot_handle.slot_id < root_playback_lookup.size()) {
            root_playback_lookup[p_bus_slot_handle.slot_id] = InvalidPlaybackSlotId;
        }
        return;
    }

    BusSlot *slot = get_bus_slot_locked(normalized);
    if (slot != nullptr && p_bus_slot_handle.slot_id < slot->playback_lookup.size()) {
        slot->playback_lookup[p_bus_slot_handle.slot_id] = InvalidPlaybackSlotId;
    }
}

void Lowl::Audio::AudioSpace::map_bus_handle_locked(const AudioBusHandle p_audio_bus_handle,
                                                    const AudioBusSlotHandle p_bus_slot_handle,
                                                    const AudioBusId p_bus_id) {
    if (!p_bus_slot_handle.is_valid() || p_bus_id == InvalidBusSlotId) {
        return;
    }

    const size_t required_size = static_cast<size_t>(p_bus_slot_handle.slot_id) + 1;
    const AudioBusHandle normalized = normalize_bus_handle_locked(p_audio_bus_handle);
    if (normalized.id == MasterBusSlotId) {
        if (root_bus_lookup.size() < required_size) {
            root_bus_lookup.resize(required_size, InvalidBusSlotId);
        }
        root_bus_lookup[p_bus_slot_handle.slot_id] = p_bus_id;
        return;
    }

    BusSlot *slot = get_bus_slot_locked(normalized);
    if (slot == nullptr) {
        return;
    }
    if (slot->child_bus_lookup.size() < required_size) {
        slot->child_bus_lookup.resize(required_size, InvalidBusSlotId);
    }
    slot->child_bus_lookup[p_bus_slot_handle.slot_id] = p_bus_id;
}

void Lowl::Audio::AudioSpace::unmap_bus_handle_locked(const AudioBusHandle p_audio_bus_handle,
                                                      const AudioBusSlotHandle p_bus_slot_handle) {
    if (!p_bus_slot_handle.is_valid()) {
        return;
    }

    const AudioBusHandle normalized = normalize_bus_handle_locked(p_audio_bus_handle);
    if (normalized.id == MasterBusSlotId) {
        if (p_bus_slot_handle.slot_id < root_bus_lookup.size()) {
            root_bus_lookup[p_bus_slot_handle.slot_id] = InvalidBusSlotId;
        }
        return;
    }

    BusSlot *slot = get_bus_slot_locked(normalized);
    if (slot != nullptr && p_bus_slot_handle.slot_id < slot->child_bus_lookup.size()) {
        slot->child_bus_lookup[p_bus_slot_handle.slot_id] = InvalidBusSlotId;
    }
}

void Lowl::Audio::AudioSpace::map_stream_handle_locked(const AudioBusHandle p_audio_bus_handle,
                                                       const AudioBusSlotHandle p_bus_slot_handle,
                                                       const AudioStreamId p_stream_slot_id) {
    if (!p_bus_slot_handle.is_valid() || p_stream_slot_id == InvalidStreamSlotId) {
        return;
    }

    const size_t required_size = static_cast<size_t>(p_bus_slot_handle.slot_id) + 1;
    const AudioBusHandle normalized = normalize_bus_handle_locked(p_audio_bus_handle);
    if (normalized.id == MasterBusSlotId) {
        if (root_stream_lookup.size() < required_size) {
            root_stream_lookup.resize(required_size, InvalidStreamSlotId);
        }
        root_stream_lookup[p_bus_slot_handle.slot_id] = p_stream_slot_id;
        return;
    }

    BusSlot *slot = get_bus_slot_locked(normalized);
    if (slot == nullptr) {
        return;
    }
    if (slot->stream_lookup.size() < required_size) {
        slot->stream_lookup.resize(required_size, InvalidStreamSlotId);
    }
    slot->stream_lookup[p_bus_slot_handle.slot_id] = p_stream_slot_id;
}

void Lowl::Audio::AudioSpace::unmap_stream_handle_locked(const AudioBusHandle p_audio_bus_handle,
                                                         const AudioBusSlotHandle p_bus_slot_handle) {
    if (!p_bus_slot_handle.is_valid()) {
        return;
    }

    const AudioBusHandle normalized = normalize_bus_handle_locked(p_audio_bus_handle);
    if (normalized.id == MasterBusSlotId) {
        if (p_bus_slot_handle.slot_id < root_stream_lookup.size()) {
            root_stream_lookup[p_bus_slot_handle.slot_id] = InvalidStreamSlotId;
        }
        return;
    }

    BusSlot *slot = get_bus_slot_locked(normalized);
    if (slot != nullptr && p_bus_slot_handle.slot_id < slot->stream_lookup.size()) {
        slot->stream_lookup[p_bus_slot_handle.slot_id] = InvalidStreamSlotId;
    }
}

Lowl::AudioStreamId
Lowl::Audio::AudioSpace::find_stream_slot_id_by_bus_slot_handle_locked(const AudioBusHandle p_audio_bus_handle,
                                                                     const AudioBusSlotHandle p_bus_slot_handle) const {
    if (!p_bus_slot_handle.is_valid()) {
        return InvalidStreamSlotId;
    }
    const AudioBusHandle normalized = normalize_bus_handle_locked(p_audio_bus_handle);
    const std::vector<AudioStreamId> *lookup = nullptr;
    if (normalized.id == MasterBusSlotId) {
        lookup = &root_stream_lookup;
    } else {
        const BusSlot *slot = get_bus_slot_locked(normalized);
        if (slot == nullptr) {
            return InvalidStreamSlotId;
        }
        lookup = &slot->stream_lookup;
    }
    if (p_bus_slot_handle.slot_id >= lookup->size()) {
        return InvalidStreamSlotId;
    }
    const AudioStreamId stream_slot_id = (*lookup)[p_bus_slot_handle.slot_id];
    if (stream_slot_id == InvalidStreamSlotId || stream_slot_id >= stream_lookup.size()) {
        return InvalidStreamSlotId;
    }
    const StreamSlot &slot = stream_lookup[stream_slot_id];
    return slot.bus_slot_handle == p_bus_slot_handle ? stream_slot_id : InvalidStreamSlotId;
}

bool Lowl::Audio::AudioSpace::bus_has_children_locked(const AudioBusHandle p_audio_bus_handle) const {
    const AudioBusHandle normalized = normalize_bus_handle_locked(p_audio_bus_handle);

    const auto has_entries = [](const auto &lookup, const auto invalid_value) {
        for (const auto value : lookup) {
            if (value != invalid_value) {
                return true;
            }
        }
        return false;
    };

    if (normalized.id == MasterBusSlotId) {
        return has_entries(root_playback_lookup, InvalidPlaybackSlotId) ||
               has_entries(root_bus_lookup, InvalidBusSlotId) ||
               has_entries(root_stream_lookup, InvalidStreamSlotId);
    }

    const BusSlot *slot = get_bus_slot_locked(normalized);
    if (slot == nullptr) {
        return false;
    }
    return has_entries(slot->playback_lookup, InvalidPlaybackSlotId) ||
           has_entries(slot->child_bus_lookup, InvalidBusSlotId) ||
           has_entries(slot->stream_lookup, InvalidStreamSlotId);
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
            const size_t max_lookup_size = static_cast<size_t>(std::numeric_limits<AudioAssetId>::max()) + 1;
            audio_asset_lookup.resize(std::min(required_size + LookupGrowth, max_lookup_size));
        }
        current_audio_asset_id = advance_allocation_id(asset_id);
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

Lowl::AudioBusHandle Lowl::Audio::AudioSpace::insert_bus_locked(AudioBusHandle p_parent_bus_handle) {
    const AudioBusHandle parent_bus_handle = normalize_bus_handle_locked(p_parent_bus_handle);
    AudioBus *parent_bus = get_bus_node_locked(parent_bus_handle);
    if (parent_bus == nullptr) {
        return InvalidAudioBusHandle;
    }

    const AudioBusSlotHandle bus_slot_handle = parent_bus->allocate_handle();
    if (!bus_slot_handle.is_valid()) {
        return InvalidAudioBusHandle;
    }

    AudioBusId bus_id = InvalidBusSlotId;
    const bool reusing_slot = !free_bus_slots.empty();
    if (reusing_slot) {
        bus_id = free_bus_slots.back();
    } else {
        if (current_audio_bus_slot_id == InvalidBusSlotId) {
            parent_bus->release_handle(bus_slot_handle);
            return InvalidAudioBusHandle;
        }
        bus_id = current_audio_bus_slot_id;
        const size_t required_size = static_cast<size_t>(bus_id) + 1;
        if (bus_lookup.size() < required_size) {
            const size_t max_lookup_size = static_cast<size_t>(std::numeric_limits<AudioBusId>::max()) + 1;
            bus_lookup.resize(std::min(required_size + LookupGrowth, max_lookup_size));
        }
    }

    if (bus_id == InvalidBusSlotId || bus_id >= bus_lookup.size()) {
        parent_bus->release_handle(bus_slot_handle);
        return InvalidAudioBusHandle;
    }

    auto bus = std::make_unique<AudioBus>(get_audio_format());
    if (reusing_slot) {
        free_bus_slots.pop_back();
    } else {
        current_audio_bus_slot_id = advance_allocation_id(bus_id);
    }

    BusSlot &slot = bus_lookup[bus_id];
    slot.bus = std::move(bus);
    slot.parent_bus_handle = parent_bus_handle;
    slot.bus_slot_handle = bus_slot_handle;
    if (slot.generation == 0) {
        slot.generation = 1;
    }
    slot.slot_state = SlotState::Active;
    slot.bus_submission_started = true;
    slot.playback_lookup.clear();
    slot.child_bus_lookup.clear();

    parent_bus->submit(bus_slot_handle, slot.bus.get());
    map_bus_handle_locked(parent_bus_handle, bus_slot_handle, bus_id);

    AudioBusHandle handle{};
    handle.owner_id = owner_id;
    handle.id = bus_id;
    handle.generation = slot.generation;
    return handle;
}

Lowl::AudioPlaybackHandle Lowl::Audio::AudioSpace::insert_playback_locked(std::unique_ptr<AudioVoice> p_voice,
                                                                          const AudioAssetHandle p_audio_asset_handle,
                                                                          AudioBusHandle p_audio_bus_handle) {
    if (!p_voice) {
        return InvalidAudioPlaybackHandle;
    }

    const AudioBusHandle audio_bus_handle = normalize_bus_handle_locked(p_audio_bus_handle);
    AudioBus *bus_node = get_bus_node_locked(audio_bus_handle);
    if (bus_node == nullptr) {
        return InvalidAudioPlaybackHandle;
    }

    const AudioBusSlotHandle bus_slot_handle = bus_node->allocate_handle();
    if (!bus_slot_handle.is_valid()) {
        return InvalidAudioPlaybackHandle;
    }

    AudioPlaybackId slot_id = InvalidPlaybackSlotId;
    const bool reusing_slot = !free_playback_slots.empty();
    if (!free_playback_slots.empty()) {
        slot_id = free_playback_slots.back();
    } else {
        if (current_audio_playback_slot_id == InvalidPlaybackSlotId) {
            bus_node->release_handle(bus_slot_handle);
            return InvalidAudioPlaybackHandle;
        }
        slot_id = current_audio_playback_slot_id;
        const size_t required_size = static_cast<size_t>(slot_id) + 1;
        if (playback_lookup.size() < required_size) {
            const size_t max_lookup_size = static_cast<size_t>(std::numeric_limits<AudioPlaybackId>::max()) + 1;
            playback_lookup.resize(std::min(required_size + LookupGrowth, max_lookup_size));
        }
    }

    if (slot_id == InvalidPlaybackSlotId || slot_id >= playback_lookup.size()) {
        bus_node->release_handle(bus_slot_handle);
        return InvalidAudioPlaybackHandle;
    }
    if (reusing_slot) {
        free_playback_slots.pop_back();
    } else {
        current_audio_playback_slot_id = advance_allocation_id(slot_id);
    }
    PlaybackSlot &slot = playback_lookup[slot_id];
    slot.voice = std::move(p_voice);
    slot.audio_asset_handle = p_audio_asset_handle;
    slot.audio_bus_handle = audio_bus_handle;
    slot.bus_slot_handle = bus_slot_handle;
    if (slot.generation == 0) {
        slot.generation = 1;
    }
    slot.slot_state = SlotState::Active;
    slot.bus_submission_started = false;
    map_playback_handle_locked(audio_bus_handle, bus_slot_handle, slot_id);

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

void Lowl::Audio::AudioSpace::recycle_bus_locked(const AudioBusId p_bus_id, BusSlot &p_slot) {
    unmap_bus_handle_locked(p_slot.parent_bus_handle, p_slot.bus_slot_handle);
    if (p_slot.bus_slot_handle.is_valid()) {
        AudioBus *parent_bus = get_bus_node_locked(p_slot.parent_bus_handle);
        if (parent_bus != nullptr) {
            parent_bus->release_handle(p_slot.bus_slot_handle);
        }
    }
    p_slot.bus.reset();
    p_slot.parent_bus_handle = InvalidAudioBusHandle;
    p_slot.bus_slot_handle = {};
    p_slot.slot_state = SlotState::Active;
    p_slot.bus_submission_started = false;
    p_slot.playback_lookup.clear();
    p_slot.child_bus_lookup.clear();
    p_slot.generation = advance_generation(p_slot.generation);
    if (p_bus_id != InvalidBusSlotId) {
        free_bus_slots.push_back(p_bus_id);
    }
}

void Lowl::Audio::AudioSpace::recycle_playback_locked(const AudioPlaybackId p_slot_id, PlaybackSlot &p_slot) {
    if (p_slot.bus_slot_handle.is_valid()) {
        unmap_playback_handle_locked(p_slot.audio_bus_handle, p_slot.bus_slot_handle);
        AudioBus *bus_node = get_bus_node_locked(p_slot.audio_bus_handle);
        if (bus_node != nullptr) {
            bus_node->release_handle(p_slot.bus_slot_handle);
        }
        p_slot.bus_slot_handle = {};
    }
    p_slot.voice.reset();
    p_slot.audio_asset_handle = InvalidAudioAssetHandle;
    p_slot.audio_bus_handle = InvalidAudioBusHandle;
    p_slot.slot_state = SlotState::Active;
    p_slot.bus_submission_started = false;
    p_slot.generation = advance_generation(p_slot.generation);
    if (p_slot_id != InvalidPlaybackSlotId) {
        free_playback_slots.push_back(p_slot_id);
    }
}

void Lowl::Audio::AudioSpace::retire_bus_locked(const AudioBusId p_bus_id, BusSlot &p_slot) {
    if (!p_slot.bus || p_slot.slot_state == SlotState::Retiring || bus_has_children_locked({owner_id, p_bus_id, p_slot.generation})) {
        return;
    }
    p_slot.slot_state = SlotState::Retiring;
    AudioBus *parent_bus = get_bus_node_locked(p_slot.parent_bus_handle);
    if (parent_bus != nullptr) {
        parent_bus->remove(p_slot.bus_slot_handle, true);
    }
}

void Lowl::Audio::AudioSpace::retire_playback_locked(const AudioPlaybackId p_slot_id, PlaybackSlot &p_slot) {
    if (!p_slot.voice || p_slot.slot_state == SlotState::Retiring) {
        return;
    }

    const bool can_recycle_immediately =
        !p_slot.bus_slot_handle.is_valid() || !p_slot.bus_submission_started ||
        (p_slot.voice->is_detached() &&
         p_slot.voice->get_playback_state() != AudioVoice::PlaybackState::Playing);
    if (can_recycle_immediately) {
        recycle_playback_locked(p_slot_id, p_slot);
        return;
    }

    p_slot.voice->stop_playback();
    p_slot.slot_state = SlotState::Retiring;
    AudioBus *bus_node = get_bus_node_locked(p_slot.audio_bus_handle);
    if (bus_node != nullptr) {
        bus_node->remove(p_slot.bus_slot_handle, true);
    }
}

void Lowl::Audio::AudioSpace::drain_bus_acks_locked(const AudioBusHandle p_audio_bus_handle) {
    const AudioBusHandle bus_handle = normalize_bus_handle_locked(p_audio_bus_handle);
    AudioBus *bus_node = get_bus_node_locked(bus_handle);
    if (bus_node == nullptr) {
        return;
    }

    AudioBusAck ack = {};
    while (bus_node->try_dequeue_ack(ack)) {
        if (!ack.handle.is_valid()) {
            continue;
        }

        const AudioPlaybackId playback_id = find_playback_slot_id_by_bus_slot_handle_locked(bus_handle, ack.handle);
        if (playback_id != InvalidPlaybackSlotId && playback_id < playback_lookup.size()) {
            PlaybackSlot &slot = playback_lookup[playback_id];
            switch (ack.type) {
                case AudioBusAck::Type::Removed: {
                    if (slot.slot_state == SlotState::Retiring) {
                        recycle_playback_locked(playback_id, slot);
                    }
                    break;
                }
                case AudioBusAck::Type::Finished: {
                    if (!slot.voice) {
                        break;
                    }
                    slot.voice->stop_playback();
                    if (slot.slot_state == SlotState::Retiring) {
                        recycle_playback_locked(playback_id, slot);
                    }
                    break;
                }
                case AudioBusAck::Type::Rejected: {
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
            continue;
        }

        const AudioBusId child_bus_id = find_child_bus_slot_id_by_bus_slot_handle_locked(bus_handle, ack.handle);
        if (child_bus_id != InvalidBusSlotId && child_bus_id < bus_lookup.size()) {
            BusSlot &slot = bus_lookup[child_bus_id];
            switch (ack.type) {
                case AudioBusAck::Type::Removed:
                case AudioBusAck::Type::Rejected:
                case AudioBusAck::Type::Finished: {
                    if (slot.slot_state == SlotState::Retiring || ack.type == AudioBusAck::Type::Rejected) {
                        recycle_bus_locked(child_bus_id, slot);
                    }
                    break;
                }
            }
            continue;
        }

        const AudioStreamId stream_id = find_stream_slot_id_by_bus_slot_handle_locked(bus_handle, ack.handle);
        if (stream_id != InvalidStreamSlotId && stream_id < stream_lookup.size()) {
            StreamSlot &slot = stream_lookup[stream_id];
            switch (ack.type) {
                case AudioBusAck::Type::Removed:
                case AudioBusAck::Type::Rejected: {
                    if (slot.slot_state == SlotState::Retiring || ack.type == AudioBusAck::Type::Rejected) {
                        recycle_stream_locked(stream_id, slot);
                    }
                    break;
                }
                case AudioBusAck::Type::Finished: {
                    if (slot.slot_state == SlotState::Retiring) {
                        recycle_stream_locked(stream_id, slot);
                    }
                    break;
                }
            }
            continue;
        }
    }
}

void Lowl::Audio::AudioSpace::drain_all_bus_acks_locked() {
    drain_bus_acks_locked(get_master_bus_handle());
    for (AudioBusId bus_id = FirstDynamicBusSlotId; bus_id < bus_lookup.size(); bus_id++) {
        BusSlot &slot = bus_lookup[bus_id];
        if (!slot.bus) {
            continue;
        }
        AudioBusHandle handle{};
        handle.owner_id = owner_id;
        handle.id = bus_id;
        handle.generation = slot.generation;
        drain_bus_acks_locked(handle);
    }
}

Lowl::AudioAssetHandle Lowl::Audio::AudioSpace::add_audio(std::unique_ptr<AudioData> p_audio_data, Error &error) {
    std::shared_ptr<AudioData> audio = std::move(p_audio_data);
    if (!audio) {
        return InvalidAudioAssetHandle;
    }
    const AudioFormat &space_format = get_audio_format();
    const SampleRate rate = audio->get_audio_format().sample_rate;

    if (!Lowl::Audio::sample_rates_equal(rate, space_format.sample_rate)) {
        std::unique_ptr<AudioData> resampled = ReSamplerR8b::resample(audio, space_format.sample_rate);
        if (!resampled) {
            error.set_error(ErrorCode::Error);
            LOWL_LOG_ERROR("Lowl::Space::load resample failed.");
            return InvalidAudioAssetHandle;
        }
        audio = std::move(resampled);
    }

    const ChannelLayout layout = audio->get_audio_format().channel_layout;
    if (layout != space_format.channel_layout) {
        ChannelConverter channel_converter;
        std::unique_ptr<AudioData> converted = channel_converter.convert(space_format.channel_layout, audio, error);
        if (error.has_error()) {
            LOWL_LOG_ERROR("Lowl::Space::load channel_converter.convert() ErrCode:" +
                           std::to_string(error.get_error_code()) + " ErrText:" + error.get_error_text() +
                           ". Could not convert layout from " + layout.to_string() + " to " +
                           space_format.channel_layout.to_string() + ".");
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
    drain_all_bus_acks_locked();
    std::shared_ptr<AudioData> audio_data = get_audio_asset_locked(p_audio_asset_handle);
    if (!audio_data) {
        return InvalidAudioPlaybackHandle;
    }
    std::unique_ptr<AudioVoice> voice = std::make_unique<AudioVoice>(audio_data);
    voice->set_name(audio_data->get_name());
    return insert_playback_locked(std::move(voice), p_audio_asset_handle, get_master_bus_handle());
}

Lowl::AudioPlaybackHandle
Lowl::Audio::AudioSpace::create_playback(const AudioAssetHandle p_audio_asset_handle, AudioBusHandle p_audio_bus_handle) {
    std::lock_guard<std::mutex> lock(state_mutex);
    drain_all_bus_acks_locked();
    std::shared_ptr<AudioData> audio_data = get_audio_asset_locked(p_audio_asset_handle);
    if (!audio_data) {
        return InvalidAudioPlaybackHandle;
    }
    std::unique_ptr<AudioVoice> voice = std::make_unique<AudioVoice>(audio_data);
    voice->set_name(audio_data->get_name());
    return insert_playback_locked(std::move(voice), p_audio_asset_handle, p_audio_bus_handle);
}

Lowl::AudioPlaybackHandle Lowl::Audio::AudioSpace::play_clip(const AudioAssetHandle p_audio_asset_handle,
                                                             AudioBusHandle p_audio_bus_handle) {
    const AudioPlaybackHandle playback_handle = create_playback(p_audio_asset_handle, p_audio_bus_handle);
    if (playback_handle.is_valid()) {
        play(playback_handle);
    }
    return playback_handle;
}

void Lowl::Audio::AudioSpace::destroy_playback(const AudioPlaybackHandle p_audio_playback_handle) {
    std::lock_guard<std::mutex> lock(state_mutex);
    drain_all_bus_acks_locked();
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

void Lowl::Audio::AudioSpace::clear_all_audio() {
    std::lock_guard<std::mutex> lock(state_mutex);
    drain_all_bus_acks_locked();

    for (size_t stream_slot_index = FirstStreamSlotId; stream_slot_index < stream_lookup.size();
         stream_slot_index++) {
        const AudioStreamId stream_slot_id = static_cast<AudioStreamId>(stream_slot_index);
        StreamSlot &slot = stream_lookup[stream_slot_id];
        if (!slot.stream || slot.slot_state == SlotState::Retiring) {
            continue;
        }
        retire_stream_locked(stream_slot_id, slot);
    }

    for (size_t playback_slot_index = FirstPlaybackSlotId; playback_slot_index < playback_lookup.size();
         playback_slot_index++) {
        const AudioPlaybackId playback_slot_id = static_cast<AudioPlaybackId>(playback_slot_index);
        PlaybackSlot &slot = playback_lookup[playback_slot_id];
        if (!slot.voice || slot.slot_state == SlotState::Retiring) {
            continue;
        }
        retire_playback_locked(playback_slot_id, slot);
    }

    for (size_t audio_asset_index = FirstAudioAssetId; audio_asset_index < audio_asset_lookup.size(); audio_asset_index++) {
        const AudioAssetId audio_asset_id = static_cast<AudioAssetId>(audio_asset_index);
        AssetSlot &slot = audio_asset_lookup[audio_asset_id];
        if (!slot.audio_data) {
            continue;
        }
        recycle_audio_asset_locked(audio_asset_id, slot);
    }
}

void Lowl::Audio::AudioSpace::stop_all_audio() {
    std::lock_guard<std::mutex> lock(state_mutex);
    drain_all_bus_acks_locked();

    for (size_t stream_slot_index = FirstStreamSlotId; stream_slot_index < stream_lookup.size();
         stream_slot_index++) {
        const AudioStreamId stream_slot_id = static_cast<AudioStreamId>(stream_slot_index);
        StreamSlot &slot = stream_lookup[stream_slot_id];
        if (!slot.stream || slot.slot_state == SlotState::Retiring) {
            continue;
        }
        slot.stream->pause();
        AudioBus *bus_node = get_bus_node_locked(slot.audio_bus_handle);
        if (bus_node != nullptr) {
            bus_node->remove(slot.bus_slot_handle);
        }
    }

    for (size_t playback_slot_index = FirstPlaybackSlotId; playback_slot_index < playback_lookup.size();
         playback_slot_index++) {
        const AudioPlaybackId playback_slot_id = static_cast<AudioPlaybackId>(playback_slot_index);
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
        AudioBus *bus_node = get_bus_node_locked(slot->audio_bus_handle);
        if (bus_node != nullptr) {
            bus_node->remove(get_bus_slot_handle_locked(handle));
        }
    }
}

void Lowl::Audio::AudioSpace::play(AudioPlaybackHandle p_audio_playback_handle) {
    std::lock_guard<std::mutex> lock(state_mutex);
    drain_all_bus_acks_locked();
    PlaybackSlot *slot = get_playback_slot_locked(p_audio_playback_handle);
    if (!slot) {
        return;
    }
    AudioVoice *voice = slot->voice.get();
    voice->restart_playback();
    slot->bus_submission_started = true;
    AudioBus *bus_node = get_bus_node_locked(slot->audio_bus_handle);
    if (bus_node != nullptr) {
        bus_node->submit(get_bus_slot_handle_locked(p_audio_playback_handle), voice);
    }
}

void Lowl::Audio::AudioSpace::pause(AudioPlaybackHandle p_audio_playback_handle) {
    std::lock_guard<std::mutex> lock(state_mutex);
    PlaybackSlot *slot = get_playback_slot_locked(p_audio_playback_handle);
    if (!slot) {
        return;
    }
    AudioVoice *voice = slot->voice.get();
    voice->pause_playback();
    AudioBus *bus_node = get_bus_node_locked(slot->audio_bus_handle);
    if (bus_node != nullptr) {
        bus_node->remove(get_bus_slot_handle_locked(p_audio_playback_handle));
    }
}

void Lowl::Audio::AudioSpace::resume(AudioPlaybackHandle p_audio_playback_handle) {
    std::lock_guard<std::mutex> lock(state_mutex);
    drain_all_bus_acks_locked();
    PlaybackSlot *slot = get_playback_slot_locked(p_audio_playback_handle);
    if (!slot || slot->voice->get_playback_state() != AudioVoice::PlaybackState::Paused) {
        return;
    }
    AudioVoice *voice = slot->voice.get();
    voice->resume_playback();
    slot->bus_submission_started = true;
    AudioBus *bus_node = get_bus_node_locked(slot->audio_bus_handle);
    if (bus_node != nullptr) {
        bus_node->submit(get_bus_slot_handle_locked(p_audio_playback_handle), voice);
    }
}

void Lowl::Audio::AudioSpace::stop(AudioPlaybackHandle p_audio_playback_handle) {
    std::lock_guard<std::mutex> lock(state_mutex);
    PlaybackSlot *slot = get_playback_slot_locked(p_audio_playback_handle);
    if (!slot) {
        return;
    }
    AudioVoice *voice = slot->voice.get();
    voice->stop_playback();
    AudioBus *bus_node = get_bus_node_locked(slot->audio_bus_handle);
    if (bus_node != nullptr) {
        bus_node->remove(get_bus_slot_handle_locked(p_audio_playback_handle));
    }
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

void Lowl::Audio::AudioSpace::set_volume(AudioBusHandle p_audio_bus_handle, const Volume p_volume) {
    std::lock_guard<std::mutex> lock(state_mutex);
    AudioBus *bus_node = get_bus_node_locked(p_audio_bus_handle);
    if (bus_node != nullptr) {
        bus_node->set_volume(p_volume);
    }
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

void Lowl::Audio::AudioSpace::set_panning(AudioBusHandle p_audio_bus_handle, const Panning p_panning) {
    std::lock_guard<std::mutex> lock(state_mutex);
    AudioBus *bus_node = get_bus_node_locked(p_audio_bus_handle);
    if (bus_node != nullptr) {
        bus_node->set_panning(p_panning);
    }
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
Lowl::Audio::AudioSpace::find_playback_slot_id_by_bus_slot_handle_locked(const AudioBusHandle p_audio_bus_handle,
                                                                      const AudioBusSlotHandle p_bus_slot_handle) const {
    if (!p_bus_slot_handle.is_valid()) {
        return InvalidPlaybackSlotId;
    }
    const AudioBusHandle normalized = normalize_bus_handle_locked(p_audio_bus_handle);
    const std::vector<AudioPlaybackId> *lookup = nullptr;
    if (normalized.id == MasterBusSlotId) {
        lookup = &root_playback_lookup;
    } else {
        const BusSlot *slot = get_bus_slot_locked(normalized);
        if (slot == nullptr) {
            return InvalidPlaybackSlotId;
        }
        lookup = &slot->playback_lookup;
    }
    if (p_bus_slot_handle.slot_id >= lookup->size()) {
        return InvalidPlaybackSlotId;
    }
    const AudioPlaybackId playback_slot_id = (*lookup)[p_bus_slot_handle.slot_id];
    if (playback_slot_id == InvalidPlaybackSlotId || playback_slot_id >= playback_lookup.size()) {
        return InvalidPlaybackSlotId;
    }
    const PlaybackSlot &slot = playback_lookup[playback_slot_id];
    return slot.bus_slot_handle == p_bus_slot_handle ? playback_slot_id : InvalidPlaybackSlotId;
}

Lowl::AudioBusId
Lowl::Audio::AudioSpace::find_child_bus_slot_id_by_bus_slot_handle_locked(const AudioBusHandle p_audio_bus_handle,
                                                                 const AudioBusSlotHandle p_bus_slot_handle) const {
    if (!p_bus_slot_handle.is_valid()) {
        return InvalidBusSlotId;
    }
    const AudioBusHandle normalized = normalize_bus_handle_locked(p_audio_bus_handle);
    const std::vector<AudioBusId> *lookup = nullptr;
    if (normalized.id == MasterBusSlotId) {
        lookup = &root_bus_lookup;
    } else {
        const BusSlot *slot = get_bus_slot_locked(normalized);
        if (slot == nullptr) {
            return InvalidBusSlotId;
        }
        lookup = &slot->child_bus_lookup;
    }
    if (p_bus_slot_handle.slot_id >= lookup->size()) {
        return InvalidBusSlotId;
    }
    const AudioBusId bus_slot_id = (*lookup)[p_bus_slot_handle.slot_id];
    if (bus_slot_id == InvalidBusSlotId || bus_slot_id >= bus_lookup.size()) {
        return InvalidBusSlotId;
    }
    const BusSlot &slot = bus_lookup[bus_slot_id];
    return slot.bus_slot_handle == p_bus_slot_handle ? bus_slot_id : InvalidBusSlotId;
}

Lowl::AudioBusSlotHandle
Lowl::Audio::AudioSpace::get_bus_slot_handle_locked(const AudioPlaybackHandle p_audio_playback_handle) const {
    if (!p_audio_playback_handle.is_valid() || p_audio_playback_handle.owner_id != owner_id ||
        p_audio_playback_handle.id >= playback_lookup.size()) {
        return {};
    }
    const PlaybackSlot &slot = playback_lookup[p_audio_playback_handle.id];
    if (slot.generation == 0 || slot.generation != p_audio_playback_handle.generation) {
        return {};
    }
    return slot.bus_slot_handle;
}

Lowl::Audio::AudioSpace::BusSlot *Lowl::Audio::AudioSpace::get_bus_slot_locked(const AudioBusHandle p_audio_bus_handle) {
    if (!p_audio_bus_handle.is_valid() || p_audio_bus_handle.owner_id != owner_id || p_audio_bus_handle.id == MasterBusSlotId ||
        p_audio_bus_handle.id >= bus_lookup.size()) {
        return nullptr;
    }
    BusSlot &slot = bus_lookup[p_audio_bus_handle.id];
    if (!slot.bus || slot.slot_state == SlotState::Retiring || slot.generation != p_audio_bus_handle.generation) {
        return nullptr;
    }
    return &slot;
}

const Lowl::Audio::AudioSpace::BusSlot *
Lowl::Audio::AudioSpace::get_bus_slot_locked(const AudioBusHandle p_audio_bus_handle) const {
    if (!p_audio_bus_handle.is_valid() || p_audio_bus_handle.owner_id != owner_id || p_audio_bus_handle.id == MasterBusSlotId ||
        p_audio_bus_handle.id >= bus_lookup.size()) {
        return nullptr;
    }
    const BusSlot &slot = bus_lookup[p_audio_bus_handle.id];
    if (!slot.bus || slot.slot_state == SlotState::Retiring || slot.generation != p_audio_bus_handle.generation) {
        return nullptr;
    }
    return &slot;
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

Lowl::Audio::AudioSource::RenderResult Lowl::Audio::AudioSpace::mix_into(AudioBlockView p_block,
                                                                         const MixGainVector &p_upstream_gain) {
    if (!playback_enabled.load(std::memory_order_relaxed)) {
        return {0, RenderState::Starved};
    }
    return master_bus_node->mix_into(p_block, compose_gain_vector(p_upstream_gain));
}

Lowl::AudioBusHandle Lowl::Audio::AudioSpace::master_bus() const {
    return get_master_bus_handle();
}

Lowl::AudioBusHandle Lowl::Audio::AudioSpace::create_bus(AudioBusHandle p_parent_bus_handle) {
    std::lock_guard<std::mutex> lock(state_mutex);
    drain_all_bus_acks_locked();
    return insert_bus_locked(p_parent_bus_handle);
}

void Lowl::Audio::AudioSpace::destroy_bus(const AudioBusHandle p_audio_bus_handle) {
    std::lock_guard<std::mutex> lock(state_mutex);
    drain_all_bus_acks_locked();
    const AudioBusHandle normalized = normalize_bus_handle_locked(p_audio_bus_handle);
    if (normalized.id == MasterBusSlotId) {
        return;
    }
    BusSlot *slot = get_bus_slot_locked(normalized);
    if (slot == nullptr) {
        return;
    }
    retire_bus_locked(normalized.id, *slot);
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

Lowl::AudioStreamHandle Lowl::Audio::AudioSpace::insert_stream_locked(std::unique_ptr<AudioStream> p_stream,
                                                                      AudioBusHandle p_audio_bus_handle) {
    if (!p_stream) {
        return InvalidAudioStreamHandle;
    }

    const AudioBusHandle audio_bus_handle = normalize_bus_handle_locked(p_audio_bus_handle);
    AudioBus *bus_node = get_bus_node_locked(audio_bus_handle);
    if (bus_node == nullptr) {
        return InvalidAudioStreamHandle;
    }

    const AudioBusSlotHandle bus_slot_handle = bus_node->allocate_handle();
    if (!bus_slot_handle.is_valid()) {
        return InvalidAudioStreamHandle;
    }

    AudioStreamId slot_id = InvalidStreamSlotId;
    const bool reusing_slot = !free_stream_slots.empty();
    if (reusing_slot) {
        slot_id = free_stream_slots.back();
    } else {
        if (current_audio_stream_slot_id == InvalidStreamSlotId) {
            bus_node->release_handle(bus_slot_handle);
            return InvalidAudioStreamHandle;
        }
        slot_id = current_audio_stream_slot_id;
        const size_t required_size = static_cast<size_t>(slot_id) + 1;
        if (stream_lookup.size() < required_size) {
            const size_t max_lookup_size = static_cast<size_t>(std::numeric_limits<AudioStreamId>::max()) + 1;
            stream_lookup.resize(std::min(required_size + LookupGrowth, max_lookup_size));
        }
    }

    if (slot_id == InvalidStreamSlotId || slot_id >= stream_lookup.size()) {
        bus_node->release_handle(bus_slot_handle);
        return InvalidAudioStreamHandle;
    }
    if (reusing_slot) {
        free_stream_slots.pop_back();
    } else {
        current_audio_stream_slot_id = advance_allocation_id(slot_id);
    }

    StreamSlot &slot = stream_lookup[slot_id];
    slot.stream = std::move(p_stream);
    slot.audio_bus_handle = audio_bus_handle;
    slot.bus_slot_handle = bus_slot_handle;
    if (slot.generation == 0) {
        slot.generation = 1;
    }
    slot.slot_state = SlotState::Active;
    slot.bus_submission_started = true;

    bus_node->submit(bus_slot_handle, slot.stream.get());
    map_stream_handle_locked(audio_bus_handle, bus_slot_handle, slot_id);

    AudioStreamHandle handle{};
    handle.owner_id = owner_id;
    handle.id = slot_id;
    handle.generation = slot.generation;
    return handle;
}

void Lowl::Audio::AudioSpace::recycle_stream_locked(const AudioStreamId p_slot_id, StreamSlot &p_slot) {
    if (p_slot.bus_slot_handle.is_valid()) {
        unmap_stream_handle_locked(p_slot.audio_bus_handle, p_slot.bus_slot_handle);
        AudioBus *bus_node = get_bus_node_locked(p_slot.audio_bus_handle);
        if (bus_node != nullptr) {
            bus_node->release_handle(p_slot.bus_slot_handle);
        }
        p_slot.bus_slot_handle = {};
    }
    p_slot.stream.reset();
    p_slot.audio_bus_handle = InvalidAudioBusHandle;
    p_slot.slot_state = SlotState::Active;
    p_slot.bus_submission_started = false;
    p_slot.generation = advance_generation(p_slot.generation);
    if (p_slot_id != InvalidStreamSlotId) {
        free_stream_slots.push_back(p_slot_id);
    }
}

void Lowl::Audio::AudioSpace::retire_stream_locked(const AudioStreamId p_slot_id, StreamSlot &p_slot) {
    if (!p_slot.stream || p_slot.slot_state == SlotState::Retiring) {
        return;
    }

    const bool can_recycle_immediately = !p_slot.bus_slot_handle.is_valid() || !p_slot.bus_submission_started;
    if (can_recycle_immediately) {
        recycle_stream_locked(p_slot_id, p_slot);
        return;
    }

    p_slot.stream->pause();
    p_slot.slot_state = SlotState::Retiring;
    AudioBus *bus_node = get_bus_node_locked(p_slot.audio_bus_handle);
    if (bus_node != nullptr) {
        bus_node->remove(p_slot.bus_slot_handle, true);
    }
}

Lowl::Audio::AudioSpace::StreamSlot *
Lowl::Audio::AudioSpace::get_stream_slot_locked(const AudioStreamHandle p_audio_stream_handle) {
    if (!p_audio_stream_handle.is_valid() || p_audio_stream_handle.owner_id != owner_id ||
        p_audio_stream_handle.id >= stream_lookup.size()) {
        return nullptr;
    }
    StreamSlot &slot = stream_lookup[p_audio_stream_handle.id];
    if (!slot.stream || slot.slot_state == SlotState::Retiring ||
        slot.generation != p_audio_stream_handle.generation) {
        return nullptr;
    }
    return &slot;
}

const Lowl::Audio::AudioSpace::StreamSlot *
Lowl::Audio::AudioSpace::get_stream_slot_locked(const AudioStreamHandle p_audio_stream_handle) const {
    if (!p_audio_stream_handle.is_valid() || p_audio_stream_handle.owner_id != owner_id ||
        p_audio_stream_handle.id >= stream_lookup.size()) {
        return nullptr;
    }
    const StreamSlot &slot = stream_lookup[p_audio_stream_handle.id];
    if (!slot.stream || slot.slot_state == SlotState::Retiring ||
        slot.generation != p_audio_stream_handle.generation) {
        return nullptr;
    }
    return &slot;
}

Lowl::AudioStreamHandle Lowl::Audio::AudioSpace::create_stream(const AudioBusHandle p_audio_bus_handle,
                                                                const size_t p_ring_buffer_size) {
    std::lock_guard<std::mutex> lock(state_mutex);
    drain_all_bus_acks_locked();
    auto stream = std::make_unique<AudioStream>(get_audio_format(), p_ring_buffer_size);
    return insert_stream_locked(std::move(stream), p_audio_bus_handle);
}

void Lowl::Audio::AudioSpace::destroy_stream(const AudioStreamHandle p_audio_stream_handle) {
    std::lock_guard<std::mutex> lock(state_mutex);
    drain_all_bus_acks_locked();
    StreamSlot *slot = get_stream_slot_locked(p_audio_stream_handle);
    if (slot == nullptr) {
        return;
    }
    retire_stream_locked(p_audio_stream_handle.id, *slot);
}

Lowl::size_l Lowl::Audio::AudioSpace::write_stream_interleaved(const AudioStreamHandle p_audio_stream_handle,
                                                                const Sample *p_interleaved,
                                                                const size_t p_frame_count) {
    std::lock_guard<std::mutex> lock(state_mutex);
    StreamSlot *slot = get_stream_slot_locked(p_audio_stream_handle);
    if (slot == nullptr || !slot->stream) {
        return 0;
    }
    return slot->stream->write_interleaved(p_interleaved, p_frame_count);
}

Lowl::size_l Lowl::Audio::AudioSpace::write_stream_planar(const AudioStreamHandle p_audio_stream_handle,
                                                           const std::vector<const Sample *> &p_channels,
                                                           const size_t p_frame_count) {
    std::lock_guard<std::mutex> lock(state_mutex);
    StreamSlot *slot = get_stream_slot_locked(p_audio_stream_handle);
    if (slot == nullptr || !slot->stream) {
        return 0;
    }
    return slot->stream->write_planar(p_channels, p_frame_count);
}

void Lowl::Audio::AudioSpace::set_volume(const AudioStreamHandle p_audio_stream_handle, const Volume p_volume) {
    std::lock_guard<std::mutex> lock(state_mutex);
    StreamSlot *slot = get_stream_slot_locked(p_audio_stream_handle);
    if (slot == nullptr || !slot->stream) {
        return;
    }
    slot->stream->set_volume(p_volume);
}

void Lowl::Audio::AudioSpace::set_panning(const AudioStreamHandle p_audio_stream_handle, const Panning p_panning) {
    std::lock_guard<std::mutex> lock(state_mutex);
    StreamSlot *slot = get_stream_slot_locked(p_audio_stream_handle);
    if (slot == nullptr || !slot->stream) {
        return;
    }
    slot->stream->set_panning(p_panning);
}

void Lowl::Audio::AudioSpace::play(const AudioStreamHandle p_audio_stream_handle) {
    std::lock_guard<std::mutex> lock(state_mutex);
    StreamSlot *slot = get_stream_slot_locked(p_audio_stream_handle);
    if (slot == nullptr || !slot->stream) {
        return;
    }
    slot->stream->play();
}

void Lowl::Audio::AudioSpace::pause(const AudioStreamHandle p_audio_stream_handle) {
    std::lock_guard<std::mutex> lock(state_mutex);
    StreamSlot *slot = get_stream_slot_locked(p_audio_stream_handle);
    if (slot == nullptr || !slot->stream) {
        return;
    }
    slot->stream->pause();
}

std::map<Lowl::AudioAssetId, std::string> Lowl::Audio::AudioSpace::get_name_mapping() const {
    std::lock_guard<std::mutex> lock(state_mutex);
    std::map<AudioAssetId, std::string> map = std::map<AudioAssetId, std::string>();
    for (size_t audio_asset_index = 0; audio_asset_index < audio_asset_lookup.size(); audio_asset_index++) {
        const AudioAssetId audio_asset_id = static_cast<AudioAssetId>(audio_asset_index);
        const AssetSlot &slot = audio_asset_lookup[audio_asset_id];
        if (!slot.audio_data) {
            continue;
        }
        map.insert_or_assign(audio_asset_id, slot.audio_data->get_name());
    }
    return map;
}
