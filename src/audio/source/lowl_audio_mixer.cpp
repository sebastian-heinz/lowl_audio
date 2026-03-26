#include "lowl_audio_mixer.h"

#include <algorithm>
#include <string>

#include "audio/lowl_audio_utilities.h"
#include "lowl_logger.h"

namespace {
    Lowl::Audio::AudioBlockView make_sub_block_view(Lowl::Audio::AudioBlockView p_block,
                                                    const uint32_t p_frame_offset,
                                                    const uint32_t p_frame_count) {
        Lowl::Audio::AudioBlockView sub_block{};
        sub_block.frame_count = p_frame_count;
        sub_block.channel_count = p_block.channel_count;
        for (uint8_t channel_index = 0; channel_index < p_block.channel_count; channel_index++) {
            sub_block.channels[static_cast<size_t>(channel_index)] = p_block.channel(channel_index) + p_frame_offset;
        }
        return sub_block;
    }
} // namespace

Lowl::Audio::AudioMixer::AudioMixer(const SampleRate p_sample_rate,
                                    const AudioChannel p_channel,
                                    const uint32_t p_scratch_buffer_capacity)
    : AudioSource(p_sample_rate, p_channel),
      scratch_buffer(std::max<uint32_t>(1U, p_scratch_buffer_capacity), static_cast<uint8_t>(get_channel_num())) {
    sources.fill(ActiveSourceSlot{});
    events = std::make_unique<moodycamel::ConcurrentQueue<AudioMixerEvent>>();
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

size_t Lowl::Audio::AudioMixer::find_source_index(const AudioSource *p_audio_source) const {
    size_t remaining_active_sources = active_source_count;
    for (size_t source_index = 0; source_index < sources.size() && remaining_active_sources > 0; source_index++) {
        const ActiveSourceSlot &slot = sources[source_index];
        if (slot.source == nullptr) {
            continue;
        }
        remaining_active_sources--;
        if (slot.source == p_audio_source) {
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

void Lowl::Audio::AudioMixer::clear_ack_queue(AckOwnerSlot &p_owner_slot) {
    if (!p_owner_slot.acknowledgements) {
        return;
    }
    AudioMixerAck ack = {};
    while (p_owner_slot.acknowledgements->try_dequeue(ack)) {
    }
}

void Lowl::Audio::AudioMixer::enqueue_ack(const AudioMixerAck &p_ack) {
    if (!p_ack.handle.is_valid()) {
        return;
    }

    const size_t owner_index = static_cast<size_t>(p_ack.handle.owner_id - 1);
    if (owner_index >= ack_owners.size()) {
        return;
    }

    AckOwnerSlot &owner_slot = ack_owners[owner_index];
    if (!owner_slot.registered.load(std::memory_order_acquire) || !owner_slot.acknowledgements) {
        return;
    }
    owner_slot.acknowledgements->enqueue(p_ack);
}

void Lowl::Audio::AudioMixer::process_events() {
    AudioMixerEvent event;
    while (events->try_dequeue(event)) {
        switch (event.type) {
            case AudioMixerEvent::Type::Mix: {
                if (event.audio_source == nullptr) {
                    break;
                }
                const size_t existing_index =
                    event.handle.is_valid() ? find_source_index(event.handle) : find_source_index(event.audio_source);
                if (existing_index != InvalidSourceIndex) {
                    break;
                }
                const size_t free_index = find_free_source_index();
                if (free_index != InvalidSourceIndex) {
                    add_source(free_index, event.handle, event.audio_source);
                } else {
                    event.audio_source->on_removed_from_mixer();
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
                const size_t source_index =
                    event.handle.is_valid() ? find_source_index(event.handle) : find_source_index(event.audio_source);
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

Lowl::Audio::AudioSource::RenderResult Lowl::Audio::AudioMixer::render_mixed_block(AudioBlockView p_block) {
    AudioBlockView scratch_view = scratch_buffer.view(p_block.frame_count);
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
        scratch_buffer.clear(p_block.frame_count);
        RenderResult render_result = source->render(scratch_view);
        if (render_result.frames_produced > 0) {
            const uint32_t frames_to_mix = std::min(render_result.frames_produced, p_block.frame_count);
            for (uint8_t channel_index = 0; channel_index < p_block.channel_count; channel_index++) {
                Sample *dst = p_block.channel(channel_index);
                Sample *src = scratch_view.channel(channel_index);
                for (uint32_t frame_index = 0; frame_index < frames_to_mix; frame_index++) {
                    dst[frame_index] += src[frame_index];
                }
            }
            produced_frames = std::max(produced_frames, frames_to_mix);
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

    AudioBlockView produced_block = p_block;
    produced_block.frame_count = produced_frames;
    process_volume(produced_block);
    process_panning(produced_block);
    return {produced_frames, has_error ? RenderState::Error : RenderState::Ok};
}

Lowl::Audio::AudioSource::RenderResult Lowl::Audio::AudioMixer::render_chunked_block(AudioBlockView p_block,
                                                                                     const uint32_t p_chunk_frame_count) {
    bool has_output = false;
    bool has_sources = false;
    bool has_error = false;
    uint32_t produced_frames = 0;

    for (uint32_t frame_offset = 0; frame_offset < p_block.frame_count; frame_offset += p_chunk_frame_count) {
        const uint32_t chunk_frames = std::min<uint32_t>(p_block.frame_count - frame_offset, p_chunk_frame_count);
        const AudioBlockView output_chunk = make_sub_block_view(p_block, frame_offset, chunk_frames);
        const RenderResult chunk_result = render_mixed_block(output_chunk);
        if (chunk_result.state == RenderState::Finished) {
            continue;
        }
        has_sources = true;
        if (chunk_result.state == RenderState::Error) {
            has_error = true;
        }
        if (chunk_result.state != RenderState::Ok && chunk_result.state != RenderState::Error) {
            continue;
        }
        produced_frames = std::max(produced_frames, frame_offset + chunk_result.frames_produced);
        has_output = has_output || chunk_result.frames_produced > 0;
    }

    if (has_error) {
        return {produced_frames, RenderState::Error};
    }

    if (!has_output) {
        return {0, has_sources ? RenderState::Starved : RenderState::Finished};
    }
    return {produced_frames, RenderState::Ok};
}

Lowl::Audio::AudioSource::RenderResult Lowl::Audio::AudioMixer::render(AudioBlockView p_block) {
    process_events();

    if (!playback_enabled.load(std::memory_order_relaxed)) {
        return {0, RenderState::Starved};
    }

    const uint8_t expected_channel_count = static_cast<uint8_t>(get_channel_num());
    if (p_block.channel_count != expected_channel_count) {
        for (uint8_t channel_index = 0; channel_index < p_block.channel_count; channel_index++) {
            std::fill_n(p_block.channel(channel_index), p_block.frame_count, static_cast<Sample>(0));
        }
        return {0, RenderState::Error};
    }

    const uint32_t scratch_frames = scratch_buffer.get_frame_capacity();

    for (uint8_t channel_index = 0; channel_index < p_block.channel_count; channel_index++) {
        Sample *dst = p_block.channel(channel_index);
        std::fill_n(dst, p_block.frame_count, static_cast<Sample>(0));
    }

    if (p_block.frame_count <= scratch_frames) {
        return render_mixed_block(p_block);
    }
    return render_chunked_block(p_block, scratch_frames);
}

void Lowl::Audio::AudioMixer::mix(AudioSource *p_audio_source) {
    mix(AudioMixerHandle{}, p_audio_source);
}

void Lowl::Audio::AudioMixer::mix(const AudioMixerHandle p_handle, AudioSource *p_audio_source) {
    if (p_audio_source == nullptr) {
        return;
    }
    if (p_audio_source->get_channel() != channel) {
        LOWL_LOG_ERROR("Lowl::AudioMixer::mix: p_audio_source(" + std::to_string(p_audio_source->get_channel_num()) +
                       "ch) does not match mixer(" + std::to_string(get_channel_num()) + "ch) channel count.");
        p_audio_source->on_removed_from_mixer();
        if (p_handle.is_valid()) {
            AudioMixerAck ack = {};
            ack.type = AudioMixerAck::Type::Rejected;
            ack.handle = p_handle;
            enqueue_ack(ack);
        }
        return;
    }

    if (!Lowl::Audio::sample_rates_equal(p_audio_source->get_sample_rate(), sample_rate)) {
        LOWL_LOG_WARN("Lowl::AudioMixer::mix: p_audio_source(" + std::to_string(p_audio_source->get_sample_rate()) +
                      ") does not match mixer(" + std::to_string(sample_rate) + ") sample rate.");
    }
    AudioMixerEvent event = {};
    event.type = AudioMixerEvent::Type::Mix;
    event.handle = p_handle;
    event.audio_source = p_audio_source;
    events->enqueue(event);
}

void Lowl::Audio::AudioMixer::remove(AudioSource *p_audio_source) {
    if (p_audio_source == nullptr) {
        return;
    }
    AudioMixerEvent event = {};
    event.type = AudioMixerEvent::Type::Remove;
    event.audio_source = p_audio_source;
    events->enqueue(event);
}

void Lowl::Audio::AudioMixer::remove(const AudioMixerHandle p_handle) {
    remove(p_handle, false);
}

void Lowl::Audio::AudioMixer::remove(const AudioMixerHandle p_handle, const bool p_acknowledge_removal) {
    if (!p_handle.is_valid()) {
        return;
    }
    AudioMixerEvent event = {};
    event.type = AudioMixerEvent::Type::Remove;
    event.handle = p_handle;
    event.acknowledge_removal = p_acknowledge_removal;
    events->enqueue(event);
}

Lowl::uint16_l Lowl::Audio::AudioMixer::register_ack_owner() {
    std::lock_guard<std::mutex> lock(ack_owner_mutex);
    for (size_t owner_index = 0; owner_index < ack_owners.size(); owner_index++) {
        AckOwnerSlot &owner_slot = ack_owners[owner_index];
        if (owner_slot.registered.load(std::memory_order_acquire)) {
            continue;
        }
        if (!owner_slot.acknowledgements) {
            owner_slot.acknowledgements = std::make_unique<moodycamel::ConcurrentQueue<AudioMixerAck>>();
        }
        clear_ack_queue(owner_slot);
        owner_slot.registered.store(true, std::memory_order_release);
        return static_cast<uint16_l>(owner_index + 1);
    }
    return 0;
}

void Lowl::Audio::AudioMixer::unregister_ack_owner(const uint16_l p_owner_id) {
    if (p_owner_id == 0) {
        return;
    }

    const size_t owner_index = static_cast<size_t>(p_owner_id - 1);
    if (owner_index >= ack_owners.size()) {
        return;
    }

    std::lock_guard<std::mutex> lock(ack_owner_mutex);
    ack_owners[owner_index].registered.store(false, std::memory_order_release);
}

bool Lowl::Audio::AudioMixer::try_dequeue_ack(const uint16_l p_owner_id, AudioMixerAck &p_ack) {
    if (p_owner_id == 0) {
        return false;
    }

    const size_t owner_index = static_cast<size_t>(p_owner_id - 1);
    if (owner_index >= ack_owners.size()) {
        return false;
    }

    AckOwnerSlot &owner_slot = ack_owners[owner_index];
    if (!owner_slot.registered.load(std::memory_order_acquire) || !owner_slot.acknowledgements) {
        return false;
    }
    return owner_slot.acknowledgements->try_dequeue(p_ack);
}

Lowl::size_l Lowl::Audio::AudioMixer::get_frames_remaining() const {
    return 1;
}

Lowl::size_l Lowl::Audio::AudioMixer::get_frame_position() const {
    return 0;
}

Lowl::size_l Lowl::Audio::AudioMixer::get_frame_count() const {
    return 0;
}
