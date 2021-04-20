#include "lowl_audio_mixer.h"


#ifdef LOWL_PROFILING

#include <chrono>

#endif

Lowl::AudioMixer::AudioMixer(SampleRate p_sample_rate, Channel p_channel)
        : AudioSource(p_sample_rate, p_channel) {
    sample_rate = p_sample_rate;
    channel = p_channel;
    streams = std::vector<std::shared_ptr<AudioStream>>();
    data = std::vector<std::shared_ptr<AudioData>>();
    mixers = std::vector<std::shared_ptr<AudioMixer>>();
    events = std::make_unique<moodycamel::ConcurrentQueue<AudioMixerEvent>>();
}

bool Lowl::AudioMixer::read(Lowl::AudioFrame &audio_frame) {
#ifdef LOWL_PROFILING
    auto t1 = std::chrono::high_resolution_clock::now();
#endif

    AudioMixerEvent event;
    while (events->try_dequeue(event)) {
        switch (event.type) {
            case AudioMixerEvent::MixAudioStream: {
                std::shared_ptr<AudioStream> audio_stream = std::static_pointer_cast<AudioStream>(event.ptr);
                streams.push_back(audio_stream);
                break;
            }
            case AudioMixerEvent::MixAudioData: {
                std::shared_ptr<AudioData> audio_data = std::static_pointer_cast<AudioData>(event.ptr);
                if (!audio_data->is_in_mixer()) {
                    audio_data->set_in_mixer(true);
                    data.push_back(audio_data);
                }
                break;
            }
            case AudioMixerEvent::MixAudioMixer: {
                std::shared_ptr<AudioMixer> audio_mixer = std::static_pointer_cast<AudioMixer>(event.ptr);
                mixers.push_back(audio_mixer);
                break;
            }
        }
    }

    AudioFrame frame;
    AudioFrame mix_frame;
    bool has_output = false;
    bool has_empty_data = false;
    for (const std::shared_ptr<AudioStream> &stream : streams) {
        if (!stream->read(frame)) {
            // stream empty - streams stay connected to the mixer, more data might be pushed at any time
            continue;
        }
        mix_frame += frame;
        has_output = true;
    }

    for (const std::shared_ptr<AudioData> &audio_data : data) {
        if (!audio_data->read(frame)) {
            // data empty - data will be removed and need to be added again
            int idx = &audio_data - &data[0];
            data[idx]->set_in_mixer(false);
            data[idx] = nullptr;
            has_empty_data = true;
            continue;
        }
        mix_frame += frame;
        has_output = true;
    }
    if (has_empty_data) {
        data.erase(std::remove(data.begin(), data.end(), nullptr), data.end());
    }

    for (const std::shared_ptr<AudioMixer> &mixer : mixers) {
        if (!mixer->read(frame)) {
            // mixer empty
            continue;
        }
        mix_frame += frame;
        has_output = true;
    }

    if (!has_output) {
        return false;
    }

    if (mix_frame.left > 1.0) {
        mix_frame.left = 1.0;
    }
    if (mix_frame.right > 1.0) {
        mix_frame.right = 1.0;
    }
    audio_frame = mix_frame;

#ifdef LOWL_PROFILING
    auto t2 = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> ms_double = t2 - t1;
    double mix_duration = ms_double.count();
    if (mix_max_duration < mix_duration) {
        mix_max_duration = mix_duration;
    }
    if (mix_min_duration > mix_duration) {
        mix_min_duration = mix_duration;
    }
    mix_total_duration += mix_duration;
    mix_frame_count++;
    mix_avg_duration = mix_total_duration / mix_frame_count;
#endif

    return true;
}

void Lowl::AudioMixer::mix_stream(std::shared_ptr<AudioStream> p_audio_stream) {
    // TODO validate input stream sample rate - perhaps warning - #ifdef to enable warnings
    AudioMixerEvent event = {};
    event.type = AudioMixerEvent::MixAudioStream;
    event.ptr = p_audio_stream;
    events->enqueue(event);
}

void Lowl::AudioMixer::mix_data(std::shared_ptr<AudioData> p_audio_data) {
    // TODO validate input stream sample rate - perhaps warning - #ifdef to enable warnings
    AudioMixerEvent event = {};
    event.type = AudioMixerEvent::MixAudioData;
    event.ptr = p_audio_data;
    events->enqueue(event);
}

void Lowl::AudioMixer::mix_mixer(std::shared_ptr<AudioMixer> p_audio_mixer) {
    // TODO validate input stream sample rate - perhaps warning - #ifdef to enable warnings
    AudioMixerEvent event = {};
    event.type = AudioMixerEvent::MixAudioMixer;
    event.ptr = p_audio_mixer;
    events->enqueue(event);
}

Lowl::size_l Lowl::AudioMixer::frames_remaining() const {
    // TODO don't access lists outside of read() call as it runs on audio thread.
    // this needs to be reworked, perhaps counting needs to happen on mixer thread,
    // or when adding data via event remember the highest input and count down.
    int remaining = 0;
    for (const std::shared_ptr<AudioStream> &stream : streams) {
        if (stream->frames_remaining() > remaining) {
            remaining = stream->frames_remaining();
        }
    }
    for (const std::shared_ptr<AudioData> &audio_data : data) {
        if (audio_data->frames_remaining() > remaining) {
            remaining = audio_data->frames_remaining();
        }
    }
    for (const std::shared_ptr<AudioMixer> &mixer : mixers) {
        if (mixer->frames_remaining() > remaining) {
            remaining = mixer->frames_remaining();
        }
    }
    return remaining;
}

void Lowl::AudioMixer::mix(std::shared_ptr<AudioSource> p_audio_source) {
    std::shared_ptr<AudioMixer> mixer = std::dynamic_pointer_cast<AudioMixer>(p_audio_source);
    if (mixer) {
        mix_mixer(mixer);
        return;
    }

    std::shared_ptr<AudioData> audio_data = std::dynamic_pointer_cast<AudioData>(p_audio_source);
    if (audio_data) {
        mix_data(audio_data);
        return;
    }

    std::shared_ptr<AudioStream> stream = std::dynamic_pointer_cast<AudioStream>(p_audio_source);
    if (mixer) {
        mix_stream(stream);
        return;
    }
}
