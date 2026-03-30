#include "lowl_audio_re_sampler_r8b.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace {
    constexpr size_t LOWL_R8B_MAX_LENGTH = static_cast<size_t>(std::numeric_limits<int>::max());

    int compute_exact_output_frames(r8b::CDSPResampler24 &p_resampler,
                                    const int p_total_frames,
                                    const Lowl::SampleRate p_sample_rate_src,
                                    const Lowl::SampleRate p_sample_rate_dst) {
        const long double estimated_frames = std::ceil(
            static_cast<long double>(p_total_frames) * static_cast<long double>(p_sample_rate_dst) /
            static_cast<long double>(p_sample_rate_src));
        int low = 0;
        int high = std::max(
            1,
            static_cast<int>(
                std::min<long double>(estimated_frames, static_cast<long double>(std::numeric_limits<int>::max()))));

        while (high < std::numeric_limits<int>::max() && p_resampler.getInputRequiredForOutput(high) <= p_total_frames) {
            low = high;
            const int remaining = std::numeric_limits<int>::max() - high;
            high += std::max(1, std::min(high, remaining));
        }

        if (high == std::numeric_limits<int>::max() &&
            p_resampler.getInputRequiredForOutput(high) <= p_total_frames) {
            return -1;
        }

        while (low < high) {
            const int mid = low + (high - low + 1) / 2;
            if (p_resampler.getInputRequiredForOutput(mid) <= p_total_frames) {
                low = mid;
            } else {
                high = mid - 1;
            }
        }

        return low;
    }
} // namespace

std::unique_ptr<Lowl::Audio::AudioData> Lowl::Audio::ReSamplerR8b::resample(std::shared_ptr<AudioData> p_audio_data,
                                                                            SampleRate p_sample_rate_dst) {
    if (!p_audio_data) {
        return nullptr;
    }

    const size_t total_frames = p_audio_data->get_frame_count();
    const uint8_t num_channel = p_audio_data->get_channel_count();
    const SampleRate sample_rate_src = p_audio_data->get_sample_rate();

    if (sample_rate_src <= 0.0 || p_sample_rate_dst <= 0.0) {
        return nullptr;
    }

    if (total_frames == 0 || num_channel == 0) {
        std::unique_ptr<AudioData> audio_data =
            std::make_unique<AudioData>(std::unique_ptr<Sample[]>(), 0, p_sample_rate_dst, p_audio_data->get_channel_layout());
        audio_data->set_name(p_audio_data->get_name());
        return audio_data;
    }

    if (total_frames > LOWL_R8B_MAX_LENGTH) {
        return nullptr;
    }

    const int total_frames_int = static_cast<int>(total_frames);
    r8b::CDSPResampler24 probe_resampler(sample_rate_src, p_sample_rate_dst, total_frames_int);
    const int output_frames_int =
        compute_exact_output_frames(probe_resampler, total_frames_int, sample_rate_src, p_sample_rate_dst);
    if (output_frames_int < 0) {
        return nullptr;
    }

    const size_t output_frames = static_cast<size_t>(output_frames_int);
    if (output_frames > 0 && output_frames > std::numeric_limits<size_t>::max() / num_channel) {
        return nullptr;
    }

    std::unique_ptr<Sample[]> resample_storage;
    if (output_frames > 0) {
        resample_storage = std::make_unique<Sample[]>(output_frames * num_channel);
    }

    for (size_t current_channel = 0; current_channel < num_channel; current_channel++) {
        Sample *dst_channel = resample_storage ? resample_storage.get() + current_channel * output_frames : nullptr;
        if (dst_channel == nullptr || output_frames_int == 0) {
            continue;
        }

        const Sample *src_channel = p_audio_data->get_channel_data(static_cast<uint8_t>(current_channel));
        if (src_channel == nullptr) {
            std::fill_n(dst_channel, output_frames, static_cast<Sample>(0));
            continue;
        }

        r8b::CDSPResampler24 re_sampler(sample_rate_src, p_sample_rate_dst, total_frames_int);
        re_sampler.oneshot(src_channel, total_frames_int, dst_channel, output_frames_int);
    }

    std::unique_ptr<AudioData> audio_data = std::make_unique<AudioData>(
        std::move(resample_storage), output_frames, p_sample_rate_dst, p_audio_data->get_channel_layout());
    audio_data->set_name(p_audio_data->get_name());
    return audio_data;
}
