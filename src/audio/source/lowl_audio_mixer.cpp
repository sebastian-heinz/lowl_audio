#include "lowl_audio_mixer.h"

#include <algorithm>
#include <string>

#include "audio/lowl_audio_utilities.h"
#include "lowl_logger.h"

Lowl::Audio::AudioMixer::AudioMixer(SampleRate p_sample_rate, AudioChannel p_channel)
    : AudioSource(p_sample_rate, p_channel),
      scratch_buffer(SCRATCH_BUFFER_CAPACITY, static_cast<uint8_t>(get_channel_num())) {
    sources.fill(ActiveSourceSlot{});
    events = std::make_unique<moodycamel::ConcurrentQueue<AudioMixerEvent>>();
}

size_t Lowl::Audio::AudioMixer::find_source_index(const AudioMixerHandle p_handle) const {
    if (!p_handle.is_valid()) {
        return sources.size();
    }
    for (size_t source_index = 0; source_index < sources.size(); source_index++) {
        if (sources[source_index].handle == p_handle) {
            return source_index;
        }
    }
    return sources.size();
}

size_t Lowl::Audio::AudioMixer::find_source_index(const AudioSource *p_audio_source) const {
    for (size_t source_index = 0; source_index < sources.size(); source_index++) {
        if (sources[source_index].source == p_audio_source) {
            return source_index;
        }
    }
    return sources.size();
}

size_t Lowl::Audio::AudioMixer::find_free_source_index() const {
    for (size_t source_index = 0; source_index < sources.size(); source_index++) {
        if (sources[source_index].source == nullptr) {
            return source_index;
        }
    }
    return sources.size();
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

Lowl::Audio::AudioSource::RenderResult Lowl::Audio::AudioMixer::render(AudioBlockView p_block) {
    AudioMixerEvent event;
    while (events->try_dequeue(event)) {
        switch (event.type) {
            case AudioMixerEvent::Type::Mix: {
                if (event.audio_source == nullptr) {
                    break;
                }
                const size_t existing_index = event.handle.is_valid() ? find_source_index(event.handle)
                                                                      : find_source_index(event.audio_source);
                if (existing_index < sources.size()) {
                    break;
                }
                const size_t free_index = find_free_source_index();
                if (free_index < sources.size()) {
                    sources[free_index].handle = event.handle;
                    sources[free_index].source = event.audio_source;
                    event.audio_source->on_added_to_mixer();
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
                const size_t source_index = event.handle.is_valid() ? find_source_index(event.handle)
                                                                    : find_source_index(event.audio_source);
                if (source_index < sources.size() && sources[source_index].source != nullptr) {
                    sources[source_index].source->on_removed_from_mixer();
                    sources[source_index] = ActiveSourceSlot{};
                }
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

    if (!is_playing) {
        return {0, RenderState::Starved};
    }

    const uint8_t expected_channel_count = static_cast<uint8_t>(get_channel_num());
    if (p_block.channel_count != expected_channel_count) {
        for (uint8_t channel_index = 0; channel_index < p_block.channel_count; channel_index++) {
            std::fill_n(p_block.channel(channel_index), p_block.frame_count, static_cast<Sample>(0));
        }
        return {0, RenderState::Starved};
    }

    bool has_output = false;
    bool has_sources = false;
    uint32_t produced_frames = 0;
    AudioBlockView scratch_view = scratch_buffer.view(p_block.frame_count);
    const uint32_t scratch_frames = scratch_view.frame_count;

    for (uint8_t channel_index = 0; channel_index < p_block.channel_count; channel_index++) {
        Sample *dst = p_block.channel(channel_index);
        std::fill_n(dst, p_block.frame_count, static_cast<Sample>(0));
    }

    for (size_t source_index = 0; source_index < sources.size(); source_index++) {
        AudioSource *source = sources[source_index].source;
        if (!source) {
            continue;
        }
        has_sources = true;
        scratch_buffer.clear(scratch_frames);
        RenderResult render_result = source->render(scratch_view);
        if (render_result.frames_produced > 0) {
            const uint32_t frames_to_mix = std::min(render_result.frames_produced, scratch_frames);
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
            source->on_removed_from_mixer();
            sources[source_index] = ActiveSourceSlot{};
            if (handle.is_valid()) {
                AudioMixerAck ack = {};
                ack.type = AudioMixerAck::Type::Finished;
                ack.handle = handle;
                enqueue_ack(ack);
            }
            continue;
        }
    }

    if (!has_output) {
        return {0, has_sources ? RenderState::Starved : RenderState::Finished};
    }

    AudioBlockView produced_block = p_block;
    produced_block.frame_count = produced_frames;
    process_volume(produced_block);
    process_panning(produced_block);

    return {produced_frames, RenderState::Ok};
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
