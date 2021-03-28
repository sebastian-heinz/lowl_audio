#include "lowl_audio_mixer.h"


#ifdef LOWL_PROFILING

#include <chrono>

#endif

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

#ifdef LOWL_PROFILING
    mix_frame_count = 0;
    mix_total_duration = 0;
    mix_max_duration = 0;
    mix_min_duration = std::numeric_limits<double>::max();;
    mix_avg_duration = 0;
#endif

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

//std::shared_ptr<Lowl::AudioData> Lowl::AudioMixer::resample(std::shared_ptr<AudioData> p_audio_data) {
//
//    std::vector<AudioFrame> frames = p_audio_data->get_frames();
//    std::vector<double> r_frames = std::vector<double>();
//    std::vector<double> l_frames = std::vector<double>();
//    for (AudioFrame frame : frames) {
//
//        // TODO make frames accessable by index for looping over each channel
//        r_frames.push_back(frame.right);
//        l_frames.push_back(frame.left);
//    }
//    int size = r_frames.size();
//    double *r_ptr = r_frames.data();
//    double *l_ptr = l_frames.data();
//    r8b::CDSPResampler24 *r_sampler = new r8b::CDSPResampler24(p_audio_data->get_sample_rate(), sample_rate, size);
//    r8b::CDSPResampler24 *l_sampler = new r8b::CDSPResampler24(p_audio_data->get_sample_rate(), sample_rate, size);
//    double *r_out_ptr = new double[size];
//    double *l_out_ptr = new double[size];
//    r_sampler->oneshot(r_ptr, size, r_out_ptr, size);
//    l_sampler->oneshot(l_ptr, size, l_out_ptr, size);
//
//    std::vector<AudioFrame> frames_out = std::vector<AudioFrame>();
//    for (int i = 0; i < size; i++) {
//        AudioFrame frame = {};
//        frame.right = r_ptr[i];
//        frame.left = l_out_ptr[i];
//        frames_out.push_back(frame);
//    }
//
//    std::shared_ptr<AudioData> audio_data_out = std::make_shared<AudioData>(
//            frames_out, sample_rate, p_audio_data->get_channel()
//
//    );
//    return audio_data_out;
//}

