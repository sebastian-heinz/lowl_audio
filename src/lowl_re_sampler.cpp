#include "lowl_re_sampler.h"

Lowl::ReSampler::ReSampler(Lowl::SampleRate p_sample_rate_src, Lowl::SampleRate p_sample_rate_dst,
                           Lowl::Channel p_channel, size_t p_sample_buffer_size, double p_req_trans_band) {
    current_frame = 0;
    total_frames_in = 0;
    total_re_sampled_frames = 0;
    sample_rate_src = p_sample_rate_src;
    sample_rate_dst = p_sample_rate_dst;
    channel = p_channel;
    num_channel = Lowl::get_channel_num(channel);
    sample_buffer_size = p_sample_buffer_size;
    resample_queue = new moodycamel::ReaderWriterQueue<AudioFrame>(sample_buffer_size * 2);
    resamples = std::vector<AudioFrame>(sample_buffer_size);
    samples = std::vector<std::vector<double>>(
            Lowl::get_channel_num(channel), std::vector<double>(sample_buffer_size)
    );
    re_samplers = std::vector<std::unique_ptr<r8b::CDSPResampler24>>();
    for (int channel_num = 0; channel_num < num_channel; channel_num++) {
        std::unique_ptr<r8b::CDSPResampler24> re_sampler = std::make_unique<r8b::CDSPResampler24>(
                sample_rate_src,
                sample_rate_dst,
                sample_buffer_size,
                p_req_trans_band
        );
        re_samplers.push_back(std::move(re_sampler));
    }
}

bool Lowl::ReSampler::write(const AudioFrame &p_audio_frame, size_t p_required_frames) {
    for (int channel_num = 0; channel_num < num_channel; channel_num++) {
        samples[channel_num][current_frame] = p_audio_frame[channel_num];
    }
    total_frames_in++;
    current_frame++;
    if (current_frame < sample_buffer_size) {
        // collect more frames till buffer is filled
        return false;
    }
    current_frame = 0;

    int samples_resampled = 0;
    for (int channel_num = 0; channel_num < num_channel; channel_num++) {
        double *sample_in_ptr = samples[channel_num].data();
        double *sample_out_ptr;
        samples_resampled = re_samplers[channel_num]->process(
                sample_in_ptr, sample_buffer_size, sample_out_ptr
        );
        if (samples_resampled >= sample_buffer_size) {
            samples_resampled = sample_buffer_size;
        }
        for (int sample_num = 0; sample_num < samples_resampled; sample_num++) {
            resamples[sample_num][channel_num] = sample_out_ptr[sample_num];
        }
    }

    size_t sample_available = resample_queue->size_approx();
    for (int sample_num = 0; sample_num < samples_resampled; sample_num++) {
        if (!resample_queue->enqueue(resamples[sample_num])) {
            // TODO error
            throw "TODO Error";
            break;
        }
        sample_available++;
        total_re_sampled_frames++;
    }

    if (sample_available < p_required_frames) {
        // not enough frames produced yet
        return false;
    }
    return true;
}

bool Lowl::ReSampler::read(Lowl::AudioFrame &audio_frame) {
    if (!resample_queue->try_dequeue(audio_frame)) {
        return false;
    }
    return true;
}

void Lowl::ReSampler::finish() {
    size_t expected_frames_resampled = total_frames_in * sample_rate_dst / sample_rate_src;
    size_t frames_remaining = expected_frames_resampled - total_re_sampled_frames;
    while (!write({}, frames_remaining)) {
        // push empty frames
    }
}

std::unique_ptr<Lowl::AudioData>
Lowl::ReSampler::resample(std::shared_ptr<AudioData> p_audio_data, SampleRate p_sample_rate_dst) {

    std::vector<AudioFrame> audio_frames = p_audio_data->get_frames();
    size_t total_frames = audio_frames.size();
    int num_channel = p_audio_data->get_channel_num();
    SampleRate sample_rate_src = p_audio_data->get_sample_rate();
    size_t expected_frames = total_frames * p_sample_rate_dst / sample_rate_src;
    std::vector<AudioFrame> resamples = std::vector<AudioFrame>(expected_frames);
    std::vector<std::vector<double>> samples = std::vector<std::vector<double>>(
			num_channel, std::vector<double>(total_frames)
    );

    for (int current_channel = 0; current_channel < num_channel; current_channel++) {
        for (int current_frame = 0; current_frame < total_frames; current_frame++) {
            samples[current_channel][current_frame] = audio_frames[current_frame][current_channel];
        }
    }

    for (int current_channel = 0; current_channel < num_channel; current_channel++) {
        std::unique_ptr<r8b::CDSPResampler24> re_sampler = std::make_unique<r8b::CDSPResampler24>(
				p_audio_data->get_sample_rate(), p_sample_rate_dst, total_frames
        );
        double *sample_in_ptr = samples[current_channel].data();
        double *sample_out_ptr = new double[expected_frames];
        re_sampler->oneshot(sample_in_ptr, total_frames, sample_out_ptr, expected_frames);
        for (int current_frame = 0; current_frame < expected_frames; current_frame++) {
            resamples[current_frame][current_channel] = sample_out_ptr[current_frame];
        }
    }

    std::unique_ptr<AudioData> audio_data = std::make_unique<AudioData>(
            resamples, p_sample_rate_dst, p_audio_data->get_channel()
    );
    return audio_data;

}
