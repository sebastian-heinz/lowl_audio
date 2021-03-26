#include "lowl_audio_mixer.h"

Lowl::AudioMixer::AudioMixer(SampleRate p_sample_rate, Channel p_channel) {
    sample_rate = p_sample_rate;
    channel = p_channel;
    streams = std::vector<std::shared_ptr<AudioStream>>();
    data = std::vector<std::shared_ptr<AudioData>>();
    running = false;
    out_stream = std::make_shared<AudioStream>(sample_rate, channel);
    events = std::make_unique<moodycamel::ConcurrentQueue<AudioMixerEvent>>();
}

Lowl::AudioMixer::~AudioMixer() {
    running = false;
    if (thread.joinable()) {
        thread.join();
    }
    streams.clear();
    data.clear();
}

void Lowl::AudioMixer::start_mix() {
    if (running) {
        return;
    }
    running = true;
    // Creates the thread without using 'std::bind'
    // TODO set thread priority / make it configurable
    thread = std::thread(&AudioMixer::mix_thread, this);
}

void Lowl::AudioMixer::stop_mix() {
    if (!running) {
        return;
    }
    running = false;
    if (thread.joinable()) {
        thread.join();
    }
    streams.clear();
    data.clear();
}

Lowl::SampleRate Lowl::AudioMixer::get_sample_rate() const {
    return sample_rate;
}

bool Lowl::AudioMixer::mix_next_frame() {

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

    if (!has_output) {
        return false;
    }

    if (mix_frame.left > 1.0) {
        mix_frame.left = 1.0;
    }
    if (mix_frame.right > 1.0) {
        mix_frame.right = 1.0;
    }
    out_stream->write(mix_frame);
    return true;
}

void Lowl::AudioMixer::mix_thread() {
    while (running) {
        mix_next_frame();
        // TODO perhaps some sleep or a way to signal all inputs are exhausted / and signal for new events available
    }
}

void Lowl::AudioMixer::mix_stream(std::shared_ptr<AudioStream> p_audio_stream) {
    AudioMixerEvent event = {};
    event.type = AudioMixerEvent::MixAudioStream;
    event.ptr = p_audio_stream;
    events->enqueue(event);
    // TODO validate input stream sample rate / channels and potentially adjust
}

void Lowl::AudioMixer::mix_data(std::shared_ptr<AudioData> p_audio_data) {
    AudioMixerEvent event = {};
    event.type = AudioMixerEvent::MixAudioData;
    event.ptr = p_audio_data;
    events->enqueue(event);
    // TODO validate input stream sample rate / channels and potentially adjust
    // TODO perhaps enqueue a copy, passing same ref twice will mix two frames in a single pass..
    //  but need to find solution to cancel midway..potentially return unique id for further commands mixer->cancel(uid)
}

std::shared_ptr<Lowl::AudioStream> Lowl::AudioMixer::get_out_stream() {
    return out_stream;
}

Lowl::Channel Lowl::AudioMixer::get_channel() const {
    return channel;
}

void Lowl::AudioMixer::mix_all() {
    while (mix_next_frame()) {
        // keep mixing
    }
}

