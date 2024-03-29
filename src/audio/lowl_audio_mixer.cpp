#include "lowl_audio_mixer.h"

#include "lowl_logger.h"

#include <string>

Lowl::Audio::AudioMixer::AudioMixer(SampleRate p_sample_rate, AudioChannel p_channel) : AudioSource(p_sample_rate,
                                                                                                    p_channel) {
    sources = std::vector<std::shared_ptr<AudioSource>>();
    events = std::make_unique<moodycamel::ConcurrentQueue<AudioMixerEvent>>();
    read_frame = {};
}

Lowl::Audio::AudioSource::ReadResult Lowl::Audio::AudioMixer::read(Audio::AudioFrame &audio_frame) {
    AudioMixerEvent event;
    while (events->try_dequeue(event)) {
        switch (event.type) {
            case AudioMixerEvent::Mix: {
                sources.erase(std::remove(sources.begin(), sources.end(), event.audio_source), sources.end());
                sources.push_back(event.audio_source);
                break;
            }
            case AudioMixerEvent::Remove: {
                sources.erase(std::remove(sources.begin(), sources.end(), event.audio_source), sources.end());
                break;
            }
        }
    }

    if (!is_playing) {
        return ReadResult::Pause;
    }

    bool has_output = false;
    bool has_empty_data = false;
    ReadResult read_result;
    audio_frame = {};

    for (const std::shared_ptr<AudioSource> &source : sources) {
        read_result = source->read(read_frame);
        if (read_result == ReadResult::Read) {
            audio_frame += read_frame;
            has_output = true;
        } else if (read_result == ReadResult::End) {
            continue;
        } else if (read_result == ReadResult::Pause) {
            continue;
        } else if (read_result == ReadResult::Remove) {
            long idx = &source - &sources[0];
            if (idx < 0 && idx >= sources.size()) {
                continue;
            }
            unsigned long ul_idx = static_cast<unsigned long>(idx);
            sources[ul_idx] = nullptr;
            has_empty_data = true;
            continue;
        }
    }

    if (has_empty_data) {
        sources.erase(std::remove(sources.begin(), sources.end(), nullptr), sources.end());
    }

    if (!has_output) {
        return ReadResult::End;
    }

    process_volume(audio_frame);
    process_panning(audio_frame);

    return ReadResult::Read;
}

void Lowl::Audio::AudioMixer::mix(std::shared_ptr<AudioSource> p_audio_source) {
    if (p_audio_source == nullptr) {
        return;
    }
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wfloat-equal"
    if (p_audio_source->get_sample_rate() != sample_rate) {
#pragma clang diagnostic pop
        LOWL_LOG_WARN("Lowl::AudioMixer::mix: p_audio_source(" + std::to_string(sample_rate) +
                      ") does not match mixer(" + std::to_string(sample_rate) + ") sample rate.");
    }
    AudioMixerEvent event = {};
    event.type = AudioMixerEvent::Mix;
    event.audio_source = p_audio_source;
    events->enqueue(event);
}

void Lowl::Audio::AudioMixer::remove(std::shared_ptr<AudioSource> p_audio_source) {
    if (p_audio_source == nullptr) {
        return;
    }
    AudioMixerEvent event = {};
    event.type = AudioMixerEvent::Remove;
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
