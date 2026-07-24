#include "audio/simd/lowl_audio_simd.h"

#include "audio/convert/lowl_audio_sample_converter.h"

void Lowl::Audio::Simd::mix_scaled_channel_scalar(const Sample *LOWL_RESTRICT p_src,
                                                  Sample *LOWL_RESTRICT p_dst,
                                                  const Sample p_gain,
                                                  const uint32_t p_frame_count) {
    if (p_src == nullptr || p_dst == nullptr) {
        return;
    }

    for (uint32_t frame_index = 0; frame_index < p_frame_count; frame_index++) {
        p_dst[frame_index] += p_src[frame_index] * p_gain;
    }
}

void Lowl::Audio::Simd::interleave_stereo_float32_scalar(const Sample *LOWL_RESTRICT p_src_l,
                                                         const Sample *LOWL_RESTRICT p_src_r,
                                                         float *LOWL_RESTRICT p_dst,
                                                         const uint32_t p_frame_count) {
    if (p_src_l == nullptr || p_src_r == nullptr || p_dst == nullptr) {
        return;
    }

    for (uint32_t frame_index = 0; frame_index < p_frame_count; frame_index++) {
        p_dst[frame_index * 2] = SampleConverter::sample_to_float(p_src_l[frame_index]);
        p_dst[frame_index * 2 + 1] = SampleConverter::sample_to_float(p_src_r[frame_index]);
    }
}

void Lowl::Audio::Simd::interleave_stereo_int16_scalar(const Sample *LOWL_RESTRICT p_src_l,
                                                       const Sample *LOWL_RESTRICT p_src_r,
                                                       int16_t *LOWL_RESTRICT p_dst,
                                                       const uint32_t p_frame_count) {
    if (p_src_l == nullptr || p_src_r == nullptr || p_dst == nullptr) {
        return;
    }

    for (uint32_t frame_index = 0; frame_index < p_frame_count; frame_index++) {
        p_dst[frame_index * 2] = SampleConverter::sample_to_int16(p_src_l[frame_index]);
        p_dst[frame_index * 2 + 1] = SampleConverter::sample_to_int16(p_src_r[frame_index]);
    }
}

Lowl::Audio::Simd::Dispatch Lowl::Audio::Simd::make_scalar_dispatch() {
    Dispatch dispatch{};
    dispatch.mix_scaled_channel = mix_scaled_channel_scalar;
    dispatch.interleave_stereo_float32 = interleave_stereo_float32_scalar;
    dispatch.interleave_stereo_int16 = interleave_stereo_int16_scalar;
    return dispatch;
}
