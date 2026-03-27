#include "lowl_audio_re_sampler_r8b.h"

std::unique_ptr<Lowl::Audio::AudioData> Lowl::Audio::ReSamplerR8b::resample(std::shared_ptr<AudioData> p_audio_data,
                                                                            SampleRate p_sample_rate_dst) {
    const size_t total_frames = p_audio_data->get_frame_count();
    const uint8_t num_channel = p_audio_data->get_channel_count();
    SampleRate sample_rate_src = p_audio_data->get_sample_rate();
    size_t expected_frames =
        static_cast<size_t>(static_cast<double>(total_frames) * p_sample_rate_dst / sample_rate_src);
    std::unique_ptr<Sample[]> resample_storage;
    if (expected_frames > 0 && num_channel > 0) {
        resample_storage = std::make_unique<Sample[]>(expected_frames * num_channel);
    }

    for (size_t current_channel = 0; current_channel < num_channel; current_channel++) {
        std::vector<double> samples(total_frames);
        const Sample *src_channel = p_audio_data->get_channel_data(static_cast<uint8_t>(current_channel));
        for (size_t current_frame = 0; current_frame < total_frames; current_frame++) {
            samples[current_frame] = src_channel ? src_channel[current_frame] : static_cast<Sample>(0);
        }
        std::unique_ptr<r8b::CDSPResampler24> re_sampler =
            std::make_unique<r8b::CDSPResampler24>(p_audio_data->get_sample_rate(), p_sample_rate_dst, total_frames);
        double *sample_in_ptr = samples.data();
        std::vector<double> sample_out(expected_frames);
        re_sampler->oneshot(
            sample_in_ptr, static_cast<int>(total_frames), sample_out.data(), static_cast<int>(expected_frames));
        Sample *dst_channel = resample_storage ? resample_storage.get() + current_channel * expected_frames : nullptr;
        for (size_t current_frame = 0; current_frame < expected_frames; current_frame++) {
            if (dst_channel != nullptr) {
                dst_channel[current_frame] = static_cast<Sample>(sample_out[current_frame]);
            }
        }
    }

    std::unique_ptr<AudioData> audio_data = std::make_unique<AudioData>(
        std::move(resample_storage), expected_frames, p_sample_rate_dst, p_audio_data->get_channel_layout());
    audio_data->set_name(p_audio_data->get_name());
    return audio_data;
}
