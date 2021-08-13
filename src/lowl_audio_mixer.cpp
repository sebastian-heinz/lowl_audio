#include "lowl_audio_mixer.h"

#include "lowl_logger.h"

#include <string>

Lowl::AudioMixer::AudioMixer(SampleRate p_sample_rate, Channel p_channel) : AudioSource(p_sample_rate, p_channel) {
    sample_rate = p_sample_rate;
    channel = p_channel;
    streams = std::vector<std::shared_ptr<AudioStream>>();
    data = std::vector<std::shared_ptr<AudioData>>();
    mixers = std::vector<std::shared_ptr<AudioMixer>>();
    events = std::make_unique<moodycamel::ConcurrentQueue<AudioMixerEvent>>();
}

bool Lowl::AudioMixer::read(Lowl::AudioFrame &audio_frame) {
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
                data.push_back(audio_data);
                break;
            }
            case AudioMixerEvent::MixAudioMixer: {
                std::shared_ptr<AudioMixer> audio_mixer = std::static_pointer_cast<AudioMixer>(event.ptr);
                mixers.push_back(audio_mixer);
                break;
            }
            case AudioMixerEvent::RemoveAudioStream: {
                std::shared_ptr<AudioStream> audio_stream = std::static_pointer_cast<AudioStream>(event.ptr);
                streams.erase(std::remove(streams.begin(), streams.end(), audio_stream), streams.end());
                break;
            }
            case AudioMixerEvent::RemoveAudioData: {
                std::shared_ptr<AudioData> audio_data = std::static_pointer_cast<AudioData>(event.ptr);
                data.erase(std::remove(data.begin(), data.end(), audio_data), data.end());
                break;
            }
            case AudioMixerEvent::RemoveAudioMixer: {
                std::shared_ptr<AudioMixer> audio_mixer = std::static_pointer_cast<AudioMixer>(event.ptr);
                mixers.erase(std::remove(mixers.begin(), mixers.end(), audio_mixer), mixers.end());
                break;
            }
        }
    }

    bool has_output = false;
    bool has_empty_data = false;
    for (const std::shared_ptr<AudioStream> &stream : streams) {
        if (!stream->read(read_frame)) {
            // stream empty - streams stay connected to the mixer, more data might be pushed at any time
            continue;
        }
        audio_frame += read_frame;
        has_output = true;
    }

    for (const std::shared_ptr<AudioData> &audio_data : data) {
        if (!audio_data->read(read_frame)) {
            // data empty - data will be removed and need to be added again
            int idx = &audio_data - &data[0];
            data[idx] = nullptr;
            has_empty_data = true;
            continue;
        }
        audio_frame += read_frame;
        has_output = true;
    }
    if (has_empty_data) {
        data.erase(std::remove(data.begin(), data.end(), nullptr), data.end());
    }

    for (const std::shared_ptr<AudioMixer> &mixer : mixers) {
        if (!mixer->read(read_frame)) {
            // mixer empty
            continue;
        }
        audio_frame += read_frame;
        has_output = true;
    }

    if (!has_output) {
        return false;
    }

    process_volume(audio_frame);
    process_panning(audio_frame);

    return true;
}

Lowl::size_l Lowl::AudioMixer::get_frames_remaining() const {
    return 1;
}

void Lowl::AudioMixer::mix_stream(std::shared_ptr<AudioStream> p_audio_stream) {
    if (p_audio_stream->get_sample_rate() != sample_rate) {
        std::string message = "Lowl::AudioMixer::mix_stream: p_audio_stream(" + std::to_string(sample_rate) +
                              ") does not match mixer(" + std::to_string(sample_rate) + ") sample rate.";
        Logger::log(Logger::Level::Warn, message);
    }
    AudioMixerEvent event = {};
    event.type = AudioMixerEvent::MixAudioStream;
    event.ptr = p_audio_stream;
    events->enqueue(event);
}

void Lowl::AudioMixer::mix_data(std::shared_ptr<AudioData> p_audio_data) {
    if (p_audio_data->get_sample_rate() != sample_rate) {
        std::string message = "Lowl::AudioMixer::mix_data: p_audio_data(" + std::to_string(sample_rate) +
                              ") does not match mixer(" + std::to_string(sample_rate) + ") sample rate.";
        Logger::log(Logger::Level::Warn, message);
    }
    AudioMixerEvent event = {};
    event.type = AudioMixerEvent::MixAudioData;
    event.ptr = p_audio_data;
    events->enqueue(event);
}

void Lowl::AudioMixer::mix_mixer(std::shared_ptr<AudioMixer> p_audio_mixer) {
    if (p_audio_mixer->get_sample_rate() != sample_rate) {
        std::string message = "Lowl::AudioMixer::mix_mixer: p_audio_mixer(" + std::to_string(sample_rate) +
                              ") does not match mixer(" + std::to_string(sample_rate) + ") sample rate.";
        Logger::log(Logger::Level::Warn, message);
    }
    AudioMixerEvent event = {};
    event.type = AudioMixerEvent::MixAudioMixer;
    event.ptr = p_audio_mixer;
    events->enqueue(event);
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
    if (stream) {
        mix_stream(stream);
        return;
    }
}

void Lowl::AudioMixer::remove_mixer(std::shared_ptr<AudioMixer> p_audio_mixer) {
    AudioMixerEvent event = {};
    event.type = AudioMixerEvent::RemoveAudioMixer;
    event.ptr = p_audio_mixer;
    events->enqueue(event);
}

void Lowl::AudioMixer::remove_data(std::shared_ptr<AudioData> p_audio_data) {
    AudioMixerEvent event = {};
    event.type = AudioMixerEvent::RemoveAudioData;
    event.ptr = p_audio_data;
    events->enqueue(event);
}

void Lowl::AudioMixer::remove_stream(std::shared_ptr<AudioStream> p_audio_stream) {
    AudioMixerEvent event = {};
    event.type = AudioMixerEvent::RemoveAudioStream;
    event.ptr = p_audio_stream;
    events->enqueue(event);
}

void Lowl::AudioMixer::remove(std::shared_ptr<AudioSource> p_audio_source) {
    std::shared_ptr<AudioMixer> mixer = std::dynamic_pointer_cast<AudioMixer>(p_audio_source);
    if (mixer) {
        remove_mixer(mixer);
        return;
    }

    std::shared_ptr<AudioData> audio_data = std::dynamic_pointer_cast<AudioData>(p_audio_source);
    if (audio_data) {
        remove_data(audio_data);
        return;
    }

    std::shared_ptr<AudioStream> stream = std::dynamic_pointer_cast<AudioStream>(p_audio_source);
    if (stream) {
        remove_stream(stream);
        return;
    }
}
