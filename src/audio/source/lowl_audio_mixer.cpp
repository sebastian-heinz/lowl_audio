#include "lowl_audio_mixer.h"

#include "lowl_logger.h"

#include <algorithm>
#include <string>

Lowl::Audio::AudioMixer::AudioMixer(
    SampleRate p_sample_rate,
    AudioChannel p_channel
) : AudioSource(p_sample_rate, p_channel),
    scratch_buffer(SCRATCH_BUFFER_CAPACITY, static_cast<uint8_t>(get_channel_num())) {
    sources.fill(nullptr);
    events = std::make_unique<moodycamel::ConcurrentQueue<AudioMixerEvent> >();
}

size_t Lowl::Audio::AudioMixer::find_source_index(const AudioSource *p_audio_source) const {
    for (size_t source_index = 0; source_index < sources.size(); source_index++) {
        if (sources[source_index] == p_audio_source) {
            return source_index;
        }
    }
    return sources.size();
}

size_t Lowl::Audio::AudioMixer::find_free_source_index() const {
    for (size_t source_index = 0; source_index < sources.size(); source_index++) {
        if (sources[source_index] == nullptr) {
            return source_index;
        }
    }
    return sources.size();
}

Lowl::Audio::AudioSource::RenderResult Lowl::Audio::AudioMixer::render(AudioBlockView p_block) {
    AudioMixerEvent event;
    while (events->try_dequeue(event)) {
        switch (event.type) {
            case AudioMixerEvent::Type::Mix: {
                if (event.audio_source == nullptr) {
                    break;
                }
                if (find_source_index(event.audio_source) < sources.size()) {
                    break;
                }
                const size_t free_index = find_free_source_index();
                if (free_index < sources.size()) {
                    sources[free_index] = event.audio_source;
                } else {
                    event.audio_source->on_removed_from_mixer();
                }
                break;
            }
            case AudioMixerEvent::Type::Remove: {
                if (event.audio_source == nullptr) {
                    break;
                }
                const size_t source_index = find_source_index(event.audio_source);
                if (source_index < sources.size()) {
                    sources[source_index]->on_removed_from_mixer();
                    sources[source_index] = nullptr;
                }
                break;
            }
        }
    }

    if (!is_playing) {
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
        AudioSource *source = sources[source_index];
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
            source->on_removed_from_mixer();
            sources[source_index] = nullptr;
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
    if (p_audio_source == nullptr) {
        return;
    }
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wfloat-equal"
    if (p_audio_source->get_sample_rate() != sample_rate) {
#pragma clang diagnostic pop
        LOWL_LOG_WARN("Lowl::AudioMixer::mix: p_audio_source(" + std::to_string(p_audio_source->get_sample_rate()) +
            ") does not match mixer(" + std::to_string(sample_rate) + ") sample rate.");
    }
    AudioMixerEvent event = {};
    event.type = AudioMixerEvent::Type::Mix;
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

Lowl::size_l Lowl::Audio::AudioMixer::get_frames_remaining() const {
    return 1;
}

Lowl::size_l Lowl::Audio::AudioMixer::get_frame_position() const {
    return 0;
}

Lowl::size_l Lowl::Audio::AudioMixer::get_frame_count() const {
    return 0;
}
