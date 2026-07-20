#include "lowl_audio_mixer.h"

#include <algorithm>
#include <limits>
#include <string>

#include "lowl_logger.h"

namespace {
    std::atomic<Lowl::uint32_l> next_audio_mixer_id{1};

    Lowl::uint16_l advance_generation(const Lowl::uint16_l p_current) {
        return p_current == std::numeric_limits<Lowl::uint16_l>::max() ? 1 : static_cast<Lowl::uint16_l>(p_current + 1);
    }

    Lowl::AudioPlaybackId advance_handle_id(const Lowl::AudioPlaybackId p_current) {
        return p_current == std::numeric_limits<Lowl::AudioPlaybackId>::max()
                   ? static_cast<Lowl::AudioPlaybackId>(0)
                   : static_cast<Lowl::AudioPlaybackId>(p_current + 1);
    }

    Lowl::uint32_l allocate_audio_mixer_id() {
        Lowl::uint32_l mixer_id = next_audio_mixer_id.fetch_add(1, std::memory_order_relaxed);
        if (mixer_id == 0) {
            mixer_id = next_audio_mixer_id.fetch_add(1, std::memory_order_relaxed);
        }
        return mixer_id;
    }
} // namespace

Lowl::Audio::AudioMixer::HandleSlot *Lowl::Audio::AudioMixer::get_handle_slot_locked(const AudioMixerHandle p_handle) {
    if (!p_handle.is_valid() || p_handle.mixer_id != mixer_id || p_handle.playback_id >= handles.size()) {
        return nullptr;
    }

    HandleSlot &slot = handles[p_handle.playback_id];
    if (!slot.allocated || slot.generation != p_handle.generation) {
        return nullptr;
    }
    return &slot;
}

Lowl::Audio::AudioMixer::AudioMixer(const AudioFormat p_audio_format)
    : AudioSource(p_audio_format),
      mixer_id(allocate_audio_mixer_id()) {
    sources.fill(ActiveSourceSlot{});
}

size_t Lowl::Audio::AudioMixer::find_source_index(const AudioMixerHandle p_handle) const {
    if (!p_handle.is_valid()) {
        return InvalidSourceIndex;
    }
    size_t remaining_active_sources = active_source_count;
    for (size_t source_index = 0; source_index < sources.size() && remaining_active_sources > 0; source_index++) {
        const ActiveSourceSlot &slot = sources[source_index];
        if (slot.source == nullptr) {
            continue;
        }
        remaining_active_sources--;
        if (slot.handle == p_handle) {
            return source_index;
        }
    }
    return InvalidSourceIndex;
}

size_t Lowl::Audio::AudioMixer::find_free_source_index() const {
    const size_t scan_limit = std::min(sources.size(), active_source_count + 1);
    for (size_t source_index = 0; source_index < scan_limit; source_index++) {
        if (sources[source_index].source == nullptr) {
            return source_index;
        }
    }
    return InvalidSourceIndex;
}

void Lowl::Audio::AudioMixer::add_source(const size_t p_source_index,
                                         const AudioMixerHandle p_handle,
                                         AudioSource *p_audio_source) {
    if (p_source_index == InvalidSourceIndex || p_audio_source == nullptr || sources[p_source_index].source != nullptr) {
        return;
    }
    sources[p_source_index].handle = p_handle;
    sources[p_source_index].source = p_audio_source;
    active_source_count++;
    p_audio_source->on_added_to_mixer();
}

void Lowl::Audio::AudioMixer::remove_source(const size_t p_source_index) {
    if (p_source_index == InvalidSourceIndex || sources[p_source_index].source == nullptr) {
        return;
    }
    sources[p_source_index].source->on_removed_from_mixer();
    sources[p_source_index] = ActiveSourceSlot{};
    active_source_count--;
}

void Lowl::Audio::AudioMixer::enqueue_ack(const AudioMixerAck &p_ack) {
    if (!p_ack.handle.is_valid()) {
        return;
    }
    if (!acknowledgements.try_enqueue(p_ack)) {
        queued_ack_overflow.store(true, std::memory_order_release);
    }
}

void Lowl::Audio::AudioMixer::process_events() {
    AudioMixerEvent event;
    while (events.try_dequeue(event)) {
        switch (event.type) {
            case AudioMixerEvent::Type::Mix: {
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
                    if (event.handle.is_valid()) {
                        AudioMixerAck ack = {};
                        ack.type = AudioMixerAck::Type::Rejected;
                        ack.handle = event.handle;
                        enqueue_ack(ack);
                    }
                }
                break;
            }
            case AudioMixerEvent::Type::Remove: {
                if (!event.handle.is_valid()) {
                    break;
                }
                const size_t source_index = find_source_index(event.handle);
                remove_source(source_index);
                if (event.acknowledge_removal && event.handle.is_valid()) {
                    AudioMixerAck ack = {};
                    ack.type = AudioMixerAck::Type::Removed;
                    ack.handle = event.handle;
                    enqueue_ack(ack);
                }
                break;
            }
        }
    }
}

Lowl::Audio::AudioSource::RenderResult
Lowl::Audio::AudioMixer::render_mixed_block(AudioBlockView p_block, const MixGainVector &p_upstream_gain) {
    uint32_t produced_frames = 0;
    bool has_output = false;
    bool has_sources = false;
    bool has_error = false;

    size_t remaining_active_sources = active_source_count;
    for (size_t source_index = 0; source_index < sources.size() && remaining_active_sources > 0; source_index++) {
        AudioSource *source = sources[source_index].source;
        if (!source) {
            continue;
        }
        remaining_active_sources--;
        has_sources = true;
        const RenderResult render_result = source->mix_into(p_block, p_upstream_gain);
        if (render_result.frames_produced > 0) {
            produced_frames = std::max(produced_frames, std::min(render_result.frames_produced, p_block.frame_count));
            has_output = true;
        } else if (render_result.state == RenderState::Finished || render_result.state == RenderState::Starved) {
            continue;
        }

        if (render_result.state == RenderState::Remove) {
            const AudioMixerHandle handle = sources[source_index].handle;
            remove_source(source_index);
            if (handle.is_valid()) {
                AudioMixerAck ack = {};
                ack.type = AudioMixerAck::Type::Finished;
                ack.handle = handle;
                enqueue_ack(ack);
            }
            continue;
        }

        if (render_result.state == RenderState::Error) {
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

Lowl::Audio::AudioSource::RenderResult Lowl::Audio::AudioMixer::mix_into(AudioBlockView p_block,
                                                                         const MixGainVector &p_upstream_gain) {
    process_events();

    if (!playback_enabled.load(std::memory_order_relaxed)) {
        return {0, RenderState::Starved};
    }

    const uint8_t expected_channel_count = get_channel_count();
    if (p_block.channel_count != expected_channel_count) {
        for (uint8_t channel_index = 0; channel_index < p_block.channel_count; channel_index++) {
            std::fill_n(p_block.channel(channel_index), p_block.frame_count, static_cast<Sample>(0));
        }
        return {0, RenderState::Error};
    }

    const MixGainVector combined_gain = compose_gain_vector(p_upstream_gain);
    return render_mixed_block(p_block, combined_gain);
}

void Lowl::Audio::AudioMixer::mix(const AudioMixerHandle p_handle, AudioSource *p_audio_source) {
    if (p_audio_source == nullptr) {
        return;
    }
    if (!p_handle.is_valid()) {
        LOWL_LOG_ERROR("Lowl::AudioMixer::mix: p_handle must be valid.");
        return;
    }

    {
        std::lock_guard<std::mutex> lock(control_mutex);
        HandleSlot *slot = get_handle_slot_locked(p_handle);
        if (slot == nullptr) {
            LOWL_LOG_ERROR("Lowl::AudioMixer::mix: p_handle is stale or not allocated.");
            enqueue_ack({AudioMixerAck::Type::Rejected, p_handle});
            return;
        }
        if (slot->bound_source != nullptr && slot->bound_source != p_audio_source) {
            LOWL_LOG_ERROR("Lowl::AudioMixer::mix: p_handle is already bound to a different source.");
            enqueue_ack({AudioMixerAck::Type::Rejected, p_handle});
            return;
        }
    }

    const AudioFormat &source_format = p_audio_source->get_audio_format();
    const AudioFormat &mixer_format = get_audio_format();
    if (source_format != mixer_format) {
        LOWL_LOG_ERROR("Lowl::AudioMixer::mix: source format(rate:" + std::to_string(source_format.sample_rate) +
                       ", layout:" + source_format.channel_layout.to_string() + ") does not match mixer(rate:" +
                       std::to_string(mixer_format.sample_rate) + ", layout:" +
                       mixer_format.channel_layout.to_string() + ").");
        enqueue_ack({AudioMixerAck::Type::Rejected, p_handle});
        return;
    }

    {
        std::lock_guard<std::mutex> lock(control_mutex);
        HandleSlot *slot = get_handle_slot_locked(p_handle);
        if (slot == nullptr) {
            LOWL_LOG_ERROR("Lowl::AudioMixer::mix: p_handle became stale before enqueue.");
            enqueue_ack({AudioMixerAck::Type::Rejected, p_handle});
            return;
        }
        const bool handle_was_unbound = slot->bound_source == nullptr;
        if (slot->bound_source == nullptr) {
            slot->bound_source = p_audio_source;
        } else if (slot->bound_source != p_audio_source) {
            LOWL_LOG_ERROR("Lowl::AudioMixer::mix: p_handle changed source before enqueue.");
            enqueue_ack({AudioMixerAck::Type::Rejected, p_handle});
            return;
        }

        AudioMixerEvent event = {};
        event.type = AudioMixerEvent::Type::Mix;
        event.handle = p_handle;
        event.audio_source = p_audio_source;
        if (!events.try_enqueue(event)) {
            if (handle_was_unbound) {
                slot->bound_source = nullptr;
                enqueue_ack({AudioMixerAck::Type::Rejected, p_handle});
            }
            LOWL_LOG_ERROR("Lowl::AudioMixer::mix: event queue is full.");
        }
    }
}

void Lowl::Audio::AudioMixer::remove(const AudioMixerHandle p_handle) {
    remove(p_handle, false);
}

void Lowl::Audio::AudioMixer::remove(const AudioMixerHandle p_handle, const bool p_acknowledge_removal) {
    if (!p_handle.is_valid()) {
        return;
    }

    {
        std::lock_guard<std::mutex> lock(control_mutex);
        if (get_handle_slot_locked(p_handle) == nullptr) {
            LOWL_LOG_ERROR("Lowl::AudioMixer::remove: p_handle is stale or not allocated.");
            if (p_acknowledge_removal) {
                enqueue_ack({AudioMixerAck::Type::Rejected, p_handle});
            }
            return;
        }

        AudioMixerEvent event = {};
        event.type = AudioMixerEvent::Type::Remove;
        event.handle = p_handle;
        event.acknowledge_removal = p_acknowledge_removal;
        if (!events.try_enqueue(event)) {
            if (p_acknowledge_removal) {
                enqueue_ack({AudioMixerAck::Type::Rejected, p_handle});
            }
            LOWL_LOG_ERROR("Lowl::AudioMixer::remove: event queue is full.");
        }
    }
}

Lowl::AudioMixerHandle Lowl::Audio::AudioMixer::allocate_handle() {
    std::lock_guard<std::mutex> lock(control_mutex);
    AudioPlaybackId handle_id = InvalidHandleId;
    if (!free_handle_ids.empty()) {
        handle_id = free_handle_ids.back();
        free_handle_ids.pop_back();
    } else {
        if (next_handle_id == InvalidHandleId) {
            return {};
        }
        handle_id = next_handle_id;
        const size_t required_size = static_cast<size_t>(handle_id) + 1;
        if (handles.size() < required_size) {
            handles.resize(required_size);
        }
        next_handle_id = advance_handle_id(handle_id);
    }

    if (handle_id == InvalidHandleId || handle_id >= handles.size()) {
        return {};
    }

    HandleSlot &slot = handles[handle_id];
    slot.allocated = true;
    slot.bound_source = nullptr;
    if (slot.generation == 0) {
        slot.generation = 1;
    }

    AudioMixerHandle handle{};
    handle.mixer_id = mixer_id;
    handle.playback_id = handle_id;
    handle.generation = slot.generation;
    return handle;
}

void Lowl::Audio::AudioMixer::release_handle(const AudioMixerHandle p_handle) {
    if (!p_handle.is_valid() || p_handle.mixer_id != mixer_id) {
        return;
    }

    std::lock_guard<std::mutex> lock(control_mutex);
    if (p_handle.playback_id >= handles.size()) {
        return;
    }

    HandleSlot &slot = handles[p_handle.playback_id];
    if (!slot.allocated || slot.generation != p_handle.generation) {
        return;
    }

    slot.allocated = false;
    slot.bound_source = nullptr;
    slot.generation = advance_generation(slot.generation);
    free_handle_ids.push_back(p_handle.playback_id);
}

bool Lowl::Audio::AudioMixer::try_dequeue_ack(AudioMixerAck &p_ack) {
    std::lock_guard<std::mutex> lock(control_mutex);
    if (acknowledgements.try_dequeue(p_ack)) {
        return true;
    }
    if (queued_ack_overflow.exchange(false, std::memory_order_acq_rel)) {
        LOWL_LOG_ERROR("Lowl::AudioMixer::try_dequeue_ack: acknowledgement queue overflowed.");
    }
    return false;
}

Lowl::size_l Lowl::Audio::AudioMixer::get_frames_remaining() const {
    return LiveFrameCountSentinel;
}

Lowl::size_l Lowl::Audio::AudioMixer::get_frame_position() const {
    return 0;
}

Lowl::size_l Lowl::Audio::AudioMixer::get_frame_count() const {
    return LiveFrameCountSentinel;
}
