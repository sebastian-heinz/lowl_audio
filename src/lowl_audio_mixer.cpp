#include "lowl_audio_mixer.h"

#include "lowl_logger.h"

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
    frames_remaining.store(0, std::memory_order_relaxed);
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

                size_l remaining = audio_stream->get_frames_remaining();
                if (remaining > frames_remaining.load(std::memory_order_relaxed)) {
                    frames_remaining.store(remaining, std::memory_order_relaxed);
                }
                break;
            }
            case AudioMixerEvent::MixAudioData: {
                std::shared_ptr<AudioData> audio_data = std::static_pointer_cast<AudioData>(event.ptr);
                if (!audio_data->is_in_mixer()) {
                    audio_data->set_in_mixer(true);
                    data.push_back(audio_data);

                    // TODO audio data can be cancelled mid way or reset, causing incorrect frame count
                    size_l remaining = audio_data->get_frames_remaining();
                    if (remaining > frames_remaining.load(std::memory_order_relaxed)) {
                        frames_remaining.store(remaining, std::memory_order_relaxed);
                    }
                }
                break;
            }
            case AudioMixerEvent::MixAudioMixer: {
                std::shared_ptr<AudioMixer> audio_mixer = std::static_pointer_cast<AudioMixer>(event.ptr);
                mixers.push_back(audio_mixer);

                size_l remaining = audio_mixer->get_frames_remaining();
                if (remaining > frames_remaining.load(std::memory_order_relaxed)) {
                    frames_remaining.store(remaining, std::memory_order_relaxed);
                }
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

    if (frames_remaining.load(std::memory_order_relaxed) > 0) {
        frames_remaining.fetch_sub(1, std::memory_order_relaxed);
    }

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
    if (p_audio_stream->get_sample_rate() != sample_rate) {
        Logger::log(Logger::Level::Warn,
                    "Lowl::AudioMixer::mix_stream: p_audio_stream(" + std::to_string(sample_rate) +
                    ") does not match mixer(" + std::to_string(sample_rate) + ") sample rate."
        );
    }
    AudioMixerEvent event = {};
    event.type = AudioMixerEvent::MixAudioStream;
    event.ptr = p_audio_stream;
    events->enqueue(event);
}

void Lowl::AudioMixer::mix_data(std::shared_ptr<AudioData> p_audio_data) {
    if (p_audio_data->get_sample_rate() != sample_rate) {
        Logger::log(Logger::Level::Warn,
                    "Lowl::AudioMixer::mix_data: p_audio_data(" + std::to_string(sample_rate) +
                    ") does not match mixer(" + std::to_string(sample_rate) + ") sample rate."
        );
    }
    AudioMixerEvent event = {};
    event.type = AudioMixerEvent::MixAudioData;
    event.ptr = p_audio_data;
    events->enqueue(event);
}

void Lowl::AudioMixer::mix_mixer(std::shared_ptr<AudioMixer> p_audio_mixer) {
    if (p_audio_mixer->get_sample_rate() != sample_rate) {
        Logger::log(Logger::Level::Warn,
                    "Lowl::AudioMixer::mix_mixer: p_audio_mixer(" + std::to_string(sample_rate) +
                    ") does not match mixer(" + std::to_string(sample_rate) + ") sample rate."
        );
    }
    AudioMixerEvent event = {};
    event.type = AudioMixerEvent::MixAudioMixer;
    event.ptr = p_audio_mixer;
    events->enqueue(event);
}

Lowl::size_l Lowl::AudioMixer::get_frames_remaining() const {
    return frames_remaining.load(std::memory_order_relaxed);
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
