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

// TODO static oneshot resampler
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
