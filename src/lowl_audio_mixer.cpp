#include "lowl_audio_mixer.h"

Lowl::AudioMixer::AudioMixer(SampleRate p_sample_rate, Channel p_channel) {
    sample_rate = p_sample_rate;
    channel = p_channel;
    streams = std::vector<std::shared_ptr<AudioStream>>();
    running = false;
    out_stream = std::make_shared<AudioStream>(sample_rate, channel);
}

Lowl::AudioMixer::~AudioMixer() {
    running = false;
    if (thread.joinable()) {
        thread.join();
    }
    streams.clear();
}

void Lowl::AudioMixer::start_mix() {
    if (running) {
        return;
    }
    running = true;
    // Creates the thread without using 'std::bind'
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
}

Lowl::SampleRate Lowl::AudioMixer::get_sample_rate() const {
    return sample_rate;
}

bool Lowl::AudioMixer::mix_next_frame() {
    AudioFrame mix_frame;
    bool has_output = false;
    for (const std::shared_ptr<AudioStream> &stream : streams) {
        AudioFrame frame;
        if (!stream->read(frame)) {
            // stream empty
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
    out_stream->write(mix_frame);
    return true;
}

void Lowl::AudioMixer::mix_thread() {
    while (running) {
        mix_next_frame();
    }
}

void Lowl::AudioMixer::mix_stream(std::shared_ptr<AudioStream> p_audio_stream) {
    // TODO validate input stream sample rate / channels and potentially adjust
    streams.push_back(std::move(p_audio_stream));
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
