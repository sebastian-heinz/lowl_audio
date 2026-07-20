#include "lowl_audio_bus.h"

#include <algorithm>
#include <limits>
#include <string>

#include "lowl_logger.h"

namespace {
    Lowl::uint16_l advance_generation(const Lowl::uint16_l p_current) {
        return p_current == std::numeric_limits<Lowl::uint16_l>::max() ? 1 : static_cast<Lowl::uint16_l>(p_current + 1);
    }

    Lowl::AudioPlaybackId advance_slot_id(const Lowl::AudioPlaybackId p_current) {
        return p_current == std::numeric_limits<Lowl::AudioPlaybackId>::max()
                   ? static_cast<Lowl::AudioPlaybackId>(0)
                   : static_cast<Lowl::AudioPlaybackId>(p_current + 1);
    }
} // namespace

Lowl::Audio::AudioBus::AudioBus(const AudioFormat p_audio_format)
    : AudioSource(p_audio_format) {
    sources.fill(ActiveSourceSlot{});
}

size_t Lowl::Audio::AudioBus::find_source_index(const AudioBusSlotHandle p_handle) const {
    if (!p_handle.is_valid()) {
        return InvalidSourceIndex;
    }
    size_t remaining = active_source_count;
    for (size_t i = 0; i < sources.size() && remaining > 0; i++) {
        const ActiveSourceSlot &slot = sources[i];
        if (slot.source == nullptr) {
            continue;
        }
        remaining--;
        if (slot.handle == p_handle) {
            return i;
        }
    }
    return InvalidSourceIndex;
}

size_t Lowl::Audio::AudioBus::find_free_source_index() const {
    const size_t scan_limit = std::min(sources.size(), active_source_count + 1);
    for (size_t i = 0; i < scan_limit; i++) {
        if (sources[i].source == nullptr) {
            return i;
        }
    }
    return InvalidSourceIndex;
}

void Lowl::Audio::AudioBus::add_source(const size_t p_source_index,
                                       const AudioBusSlotHandle p_handle,
                                       AudioSource *p_audio_source) {
    if (p_source_index == InvalidSourceIndex || p_audio_source == nullptr || sources[p_source_index].source != nullptr) {
        return;
    }
    sources[p_source_index].handle = p_handle;
    sources[p_source_index].source = p_audio_source;
    active_source_count++;
    p_audio_source->on_added_to_mixer();
}

void Lowl::Audio::AudioBus::remove_source(const size_t p_source_index) {
    if (p_source_index == InvalidSourceIndex || sources[p_source_index].source == nullptr) {
        return;
    }
    sources[p_source_index].source->on_removed_from_mixer();
    sources[p_source_index] = ActiveSourceSlot{};
    active_source_count--;
}

void Lowl::Audio::AudioBus::process_events() {
    AudioBusEvent event;
    while (events.try_dequeue(event)) {
        switch (event.type) {
            case AudioBusEvent::Type::Submit: {
                if (!event.handle.is_valid() || event.audio_source == nullptr) {
                    break;
                }
                const size_t existing_index = find_source_index(event.handle);
                if (existing_index != InvalidSourceIndex) {
                    break;
                }
                const size_t free_index = find_free_source_index();
                if (free_index != InvalidSourceIndex) {
                    add_source(free_index, event.handle, event.audio_source);
                } else {
                    AudioBusAck ack = {};
                    ack.type = AudioBusAck::Type::Rejected;
                    ack.handle = event.handle;
                    acks.try_enqueue(ack);
                }
                break;
            }
            case AudioBusEvent::Type::Remove: {
                if (!event.handle.is_valid()) {
                    break;
                }
                const size_t source_index = find_source_index(event.handle);
                remove_source(source_index);
                if (event.acknowledge_removal) {
                    AudioBusAck ack = {};
                    ack.type = AudioBusAck::Type::Removed;
                    ack.handle = event.handle;
                    acks.try_enqueue(ack);
                }
                break;
            }
        }
    }
}

Lowl::Audio::AudioSource::RenderResult
Lowl::Audio::AudioBus::render_mixed_block(AudioBlockView p_block, const MixGainVector &p_upstream_gain) {
    uint32_t produced_frames = 0;
    bool has_output = false;
    bool has_sources = false;
    bool has_error = false;

    size_t remaining = active_source_count;
    for (size_t i = 0; i < sources.size() && remaining > 0; i++) {
        AudioSource *source = sources[i].source;
        if (!source) {
            continue;
        }
        remaining--;
        has_sources = true;
        const RenderResult result = source->mix_into(p_block, p_upstream_gain);
        if (result.frames_produced > 0) {
            produced_frames = std::max(produced_frames, std::min(result.frames_produced, p_block.frame_count));
            has_output = true;
        } else if (result.state == RenderState::Finished || result.state == RenderState::Starved) {
            continue;
        }

        if (result.state == RenderState::Remove) {
            const AudioBusSlotHandle handle = sources[i].handle;
            remove_source(i);
            AudioBusAck ack = {};
            ack.type = AudioBusAck::Type::Finished;
            ack.handle = handle;
            acks.try_enqueue(ack);
            continue;
        }

        if (result.state == RenderState::Error) {
            has_error = true;
        }
    }

    if (!has_output) {
        if (has_error) {
            return {0, RenderState::Error};
        }
        return {0, has_sources ? RenderState::Starved : RenderState::Finished};
    }
    return {produced_frames, has_error ? RenderState::Error : RenderState::Ok};
}

Lowl::Audio::AudioSource::RenderResult Lowl::Audio::AudioBus::mix_into(AudioBlockView p_block,
                                                                       const MixGainVector &p_upstream_gain) {
    process_events();

    if (!playback_enabled.load(std::memory_order_relaxed)) {
        return {0, RenderState::Starved};
    }

    const uint8_t expected_channel_count = get_channel_count();
    if (p_block.channel_count != expected_channel_count) {
        for (uint8_t ch = 0; ch < p_block.channel_count; ch++) {
            std::fill_n(p_block.channel(ch), p_block.frame_count, static_cast<Sample>(0));
        }
        return {0, RenderState::Error};
    }

    const MixGainVector combined_gain = compose_gain_vector(p_upstream_gain);
    return render_mixed_block(p_block, combined_gain);
}

Lowl::AudioBusSlotHandle Lowl::Audio::AudioBus::allocate_handle() {
    AudioPlaybackId slot_id = InvalidSlotId;
    if (!free_slot_ids.empty()) {
        slot_id = free_slot_ids.back();
        free_slot_ids.pop_back();
    } else {
        if (next_slot_id == InvalidSlotId) {
            return {};
        }
        slot_id = next_slot_id;
        const size_t required_size = static_cast<size_t>(slot_id) + 1;
        if (handles.size() < required_size) {
            handles.resize(required_size);
        }
        next_slot_id = advance_slot_id(slot_id);
    }

    if (slot_id == InvalidSlotId || slot_id >= handles.size()) {
        return {};
    }

    HandleSlot &slot = handles[slot_id];
    slot.allocated = true;
    if (slot.generation == 0) {
        slot.generation = 1;
    }

    AudioBusSlotHandle handle{};
    handle.slot_id = slot_id;
    handle.generation = slot.generation;
    return handle;
}

void Lowl::Audio::AudioBus::release_handle(const AudioBusSlotHandle p_handle) {
    if (!p_handle.is_valid() || p_handle.slot_id >= handles.size()) {
        return;
    }

    HandleSlot &slot = handles[p_handle.slot_id];
    if (!slot.allocated || slot.generation != p_handle.generation) {
        return;
    }

    slot.allocated = false;
    slot.generation = advance_generation(slot.generation);
    free_slot_ids.push_back(p_handle.slot_id);
}

void Lowl::Audio::AudioBus::submit(const AudioBusSlotHandle p_handle, AudioSource *p_audio_source) {
    if (p_audio_source == nullptr || !p_handle.is_valid()) {
        return;
    }

    if (p_handle.slot_id >= handles.size()) {
        acks.try_enqueue(AudioBusAck{AudioBusAck::Type::Rejected, p_handle});
        return;
    }

    const HandleSlot &slot = handles[p_handle.slot_id];
    if (!slot.allocated || slot.generation != p_handle.generation) {
        LOWL_LOG_ERROR("Lowl::AudioBus::submit: handle is stale or not allocated.");
        acks.try_enqueue(AudioBusAck{AudioBusAck::Type::Rejected, p_handle});
        return;
    }

    const AudioFormat &source_format = p_audio_source->get_audio_format();
    const AudioFormat &bus_format = get_audio_format();
    if (source_format != bus_format) {
        LOWL_LOG_ERROR("Lowl::AudioBus::submit: source format(rate:" + std::to_string(source_format.sample_rate) +
                       ", layout:" + source_format.channel_layout.to_string() + ") does not match bus(rate:" +
                       std::to_string(bus_format.sample_rate) + ", layout:" +
                       bus_format.channel_layout.to_string() + ").");
        acks.try_enqueue(AudioBusAck{AudioBusAck::Type::Rejected, p_handle});
        return;
    }

    AudioBusEvent event = {};
    event.type = AudioBusEvent::Type::Submit;
    event.handle = p_handle;
    event.audio_source = p_audio_source;
    if (!events.try_enqueue(event)) {
        acks.try_enqueue(AudioBusAck{AudioBusAck::Type::Rejected, p_handle});
        LOWL_LOG_ERROR("Lowl::AudioBus::submit: event queue is full.");
    }
}

void Lowl::Audio::AudioBus::remove(const AudioBusSlotHandle p_handle) {
    remove(p_handle, false);
}

void Lowl::Audio::AudioBus::remove(const AudioBusSlotHandle p_handle, const bool p_acknowledge_removal) {
    if (!p_handle.is_valid()) {
        return;
    }

    if (p_handle.slot_id >= handles.size()) {
        if (p_acknowledge_removal) {
            acks.try_enqueue(AudioBusAck{AudioBusAck::Type::Rejected, p_handle});
        }
        return;
    }

    const HandleSlot &slot = handles[p_handle.slot_id];
    if (!slot.allocated || slot.generation != p_handle.generation) {
        LOWL_LOG_ERROR("Lowl::AudioBus::remove: handle is stale or not allocated.");
        if (p_acknowledge_removal) {
            acks.try_enqueue(AudioBusAck{AudioBusAck::Type::Rejected, p_handle});
        }
        return;
    }

    AudioBusEvent event = {};
    event.type = AudioBusEvent::Type::Remove;
    event.handle = p_handle;
    event.acknowledge_removal = p_acknowledge_removal;
    if (!events.try_enqueue(event)) {
        if (p_acknowledge_removal) {
            acks.try_enqueue(AudioBusAck{AudioBusAck::Type::Rejected, p_handle});
        }
        LOWL_LOG_ERROR("Lowl::AudioBus::remove: event queue is full.");
    }
}

bool Lowl::Audio::AudioBus::try_dequeue_ack(AudioBusAck &p_ack) {
    return acks.try_dequeue(p_ack);
}

Lowl::size_l Lowl::Audio::AudioBus::get_frames_remaining() const {
    return LiveFrameCountSentinel;
}

Lowl::size_l Lowl::Audio::AudioBus::get_frame_position() const {
    return 0;
}

Lowl::size_l Lowl::Audio::AudioBus::get_frame_count() const {
    return LiveFrameCountSentinel;
}
