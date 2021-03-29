#include "lowl_re_sampler.h"

Lowl::ReSampler::ReSampler(Lowl::SampleRate p_sample_rate_src, Lowl::SampleRate p_sample_rate_dst,
                           Lowl::Channel p_channel, size_t p_sample_buffer_size) {
    current_frame = 0;
    sample_available = 0;
    sample_rate_src = p_sample_rate_src;
    sample_rate_dst = p_sample_rate_dst;
    channel = p_channel;
    num_channel = Lowl::get_channel_num(channel);
    resample_queue = new moodycamel::ReaderWriterQueue<AudioFrame>(100);
    sample_buffer_size = p_sample_buffer_size;
    resamples = std::vector<AudioFrame>(sample_buffer_size);
    samples = std::vector<std::vector<double>>(
            Lowl::get_channel_num(channel), std::vector<double>(sample_buffer_size)
    );
    re_samplers = std::vector<std::unique_ptr<r8b::CDSPResampler24>>();
    for (int channel_num = 0; channel_num < num_channel; channel_num++) {
        std::unique_ptr<r8b::CDSPResampler24> re_sampler = std::make_unique<r8b::CDSPResampler24>(
                sample_rate_src,
                sample_rate_dst,
                sample_buffer_size
        );
        re_samplers.push_back(std::move(re_sampler));
    }
}

bool Lowl::ReSampler::write(const AudioFrame &p_audio_frame, size_t p_required_frames) {

    for (int channel_num = 0; channel_num < num_channel; channel_num++) {
        samples[channel_num][current_frame] = p_audio_frame[channel_num];
    }
    current_frame++;
    if (current_frame < sample_buffer_size) {
        return false;
    }
    current_frame = 0;

    int last_samples_resampled = 0;
    int total_samples_resampled = 0;
    for (int channel_num = 0; channel_num < num_channel; channel_num++) {
        double *sample_in_ptr = samples[channel_num].data();
        double *sample_out_ptr;
        last_samples_resampled = re_samplers[channel_num]->process(
                sample_in_ptr, sample_buffer_size, sample_out_ptr
        );
        if (last_samples_resampled >= sample_buffer_size) {
            // TODO err
            int i = 1;
        }
        // TODO 0 samples might be produced
        // TODO  calls resampling process until output buffer is filled
        for (int sample_num = 0; sample_num < last_samples_resampled; sample_num++) {
            resamples[sample_num][channel_num] = sample_out_ptr[sample_num];
        }
        total_samples_resampled += last_samples_resampled;
    }
    int samples_resampled = total_samples_resampled / num_channel;
    if (samples_resampled != last_samples_resampled) {
        // TODO err
        // TODO expected all channels produce same number of resamples
        int i = 1;
    }

    for (int sample_num = 0; sample_num < samples_resampled; sample_num++) {
        if (!resample_queue->try_enqueue(resamples[sample_num])) {
            // TODO error
            break;
        }
        sample_available++;
    }
    if (sample_available < p_required_frames) {
        return false;
    }

    return true;
}

bool Lowl::ReSampler::read(Lowl::AudioFrame &audio_frame) {
    if (sample_available <= 0) {
        return false;
    }
    if (!resample_queue->try_dequeue(audio_frame)) {
        return false;
    }
    sample_available--;
    return true;
}
