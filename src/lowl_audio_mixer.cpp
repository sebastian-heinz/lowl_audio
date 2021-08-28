#include "lowl_audio_mixer.h"

#include "lowl_logger.h"
#include "lowl_audio_data.h"

#include <string>

Lowl::AudioMixer::AudioMixer(SampleRate p_sample_rate, Channel p_channel) : AudioSource(p_sample_rate, p_channel) {
    sources = std::vector<std::shared_ptr<AudioSource>>();
    events = std::make_unique<moodycamel::ConcurrentQueue<AudioMixerEvent>>();
    read_frame = {};
}

Lowl::AudioSource::ReadResult Lowl::AudioMixer::read(Lowl::AudioFrame &audio_frame) {
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
            std::shared_ptr<AudioData> data = std::dynamic_pointer_cast<AudioData>(source);
            if (data) {
                // data empty - data will be removed and need to be added again
                int idx = &source - &sources[0];
                sources[idx] = nullptr;
                has_empty_data = true;
            }
            continue;
        } else if (read_result == ReadResult::Pause) {
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

void Lowl::AudioMixer::mix(std::shared_ptr<AudioSource> p_audio_source) {
    if (p_audio_source->get_sample_rate() != sample_rate) {
        std::string message = "Lowl::AudioMixer::mix: p_audio_source(" + std::to_string(sample_rate) +
                              ") does not match mixer(" + std::to_string(sample_rate) + ") sample rate.";
        Logger::log(Logger::Level::Warn, message);
    }
    AudioMixerEvent event = {};
    event.type = AudioMixerEvent::Mix;
    event.audio_source = p_audio_source;
    events->enqueue(event);
}

void Lowl::AudioMixer::remove(std::shared_ptr<AudioSource> p_audio_source) {
    AudioMixerEvent event = {};
    event.type = AudioMixerEvent::Remove;
    event.audio_source = p_audio_source;
    events->enqueue(event);
}

Lowl::size_l Lowl::AudioMixer::get_frames_remaining() const {
    return 1;
}

Lowl::size_l Lowl::AudioMixer::get_frame_position() const {
    return 0;
}
