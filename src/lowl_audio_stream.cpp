#include "lowl_audio_stream.h"

Lowl::AudioStream::AudioStream(SampleRate p_sample_rate, Channel p_channel) {
    sample_rate = p_sample_rate;
    output_sample_rate = p_sample_rate;
    channel = p_channel;
    frame_queue = new moodycamel::ReaderWriterQueue<AudioFrame>(100);
    resample_queue = new moodycamel::ReaderWriterQueue<AudioFrame>(100);
    frames_in = 0;
    frames_out = 0;
    re_samplers = std::vector<std::unique_ptr<r8b::CDSPResampler24>>();
    re_sampler_sample_buffer_size = 256;
    require_resampling = false;
}

Lowl::AudioStream::~AudioStream() {
    delete frame_queue;
}

Lowl::Channel Lowl::AudioStream::get_channel() const {
    return channel;
}

Lowl::SampleRate Lowl::AudioStream::get_sample_rate() const {
    return sample_rate;
}

Lowl::SampleFormat Lowl::AudioStream::get_sample_format() const {
    return SampleFormat::FLOAT_32;
}

bool Lowl::AudioStream::read(AudioFrame &audio_frame) {
    if (resample_queue->try_dequeue(audio_frame)) {
        // serve any remaining resamples
        frames_out++;
        return true;
    }
    while (is_sample_rate_changing.test_and_set(std::memory_order_acquire)) {
        // spin lock
    }
    if (require_resampling) {
        // produce resamples
        int channel_count = get_channel_num();
        bool frame_available = false;
        std::vector<AudioFrame> resamples = std::vector<AudioFrame>(re_sampler_sample_buffer_size);
        std::vector<std::vector<double>> samples(channel_count, std::vector<double>(re_sampler_sample_buffer_size));
        for (int sample_num = 0; sample_num < re_sampler_sample_buffer_size; sample_num++) {
            AudioFrame next_frame;
            if (!frame_queue->try_dequeue(next_frame)) {
                break;
            }
            frame_available = true;
            for (int channel_num = 0; channel_num < get_channel_num(); channel_num++) {
                samples[channel_num][sample_num] = next_frame[channel_num];
            }
        }
        if (!frame_available) {
            // no more frames in resample_queue or frame_queue
            is_sample_rate_changing.clear(std::memory_order_release);
            return false;
        }
        int samples_resampled;
        for (int channel_num = 0; channel_num < get_channel_num(); channel_num++) {
            double *sample_in_ptr = samples[channel_num].data();
            double *sample_out_ptr;
            samples_resampled = re_samplers[channel_num]->process(
                    sample_in_ptr, re_sampler_sample_buffer_size, sample_out_ptr
            );
            for (int sample_num = 0; sample_num < samples_resampled; sample_num++) {
                resamples[sample_num][channel_num] = sample_out_ptr[sample_num];
            }
        }
        for (int sample_num = 0; sample_num < samples_resampled; sample_num++) {
            if (!resample_queue->try_enqueue(resamples[sample_num])) {
                // TODO error
                break;
            }
        }
        if (resample_queue->try_dequeue(audio_frame)) {
            frames_out++;
            is_sample_rate_changing.clear(std::memory_order_release);
            return true;
        }
    }
    is_sample_rate_changing.clear(std::memory_order_release);

    // serve frame
    if (!frame_queue->try_dequeue(audio_frame)) {
        return false;
    }
    frames_out++;
    return true;
}

bool Lowl::AudioStream::write(const AudioFrame &p_audio_frame) {
    if (!frame_queue->enqueue(p_audio_frame)) {
        return false;
    }
    frames_in++;
    return true;
}

int Lowl::AudioStream::get_channel_num() const {
    return Lowl::get_channel_num(channel);
}

void Lowl::AudioStream::write(const std::vector<AudioFrame> &p_audio_frames) {
    for (const AudioFrame &frame : p_audio_frames) {
        write(frame);
    }
}

uint32_t Lowl::AudioStream::get_num_frame_read() const {
    return frames_out;
}

uint32_t Lowl::AudioStream::get_num_frame_write() const {
    return frames_in;
}

size_t Lowl::AudioStream::get_num_frame_queued() const {
    return frame_queue->size_approx();
}

std::vector<Lowl::AudioFrame> Lowl::AudioStream::read() {
    std::vector<AudioFrame> frames = std::vector<AudioFrame>();
    AudioFrame frame;
    while (read(frame)) {
        frames.push_back(frame);
    }
    return frames;
}

void Lowl::AudioStream::set_output_sample_rate(Lowl::SampleRate p_output_sample_rate) {
    while (is_sample_rate_changing.test_and_set(std::memory_order_acquire)) {
        // spin lock
    }
    if (p_output_sample_rate != sample_rate) {
        output_sample_rate = p_output_sample_rate;
        re_samplers.clear();
        for (int channel_num = 0; channel_num < get_channel_num(); channel_num++) {
            std::unique_ptr<r8b::CDSPResampler24> re_sampler = std::make_unique<r8b::CDSPResampler24>(
                    sample_rate,
                    output_sample_rate,
                    re_sampler_sample_buffer_size
            );
            re_samplers.push_back(std::move(re_sampler));
        }
        require_resampling = true;
    } else {
        re_samplers.clear();
        require_resampling = false;
    }
    is_sample_rate_changing.clear(std::memory_order_release);
}