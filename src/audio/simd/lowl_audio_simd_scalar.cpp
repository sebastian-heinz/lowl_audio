#include "audio/simd/lowl_audio_simd.h"

#include <algorithm>

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

void Lowl::Audio::Simd::interleave_stereo_float32_scalar(const float *LOWL_RESTRICT p_src_l,
                                                         const float *LOWL_RESTRICT p_src_r,
                                                         float *LOWL_RESTRICT p_dst,
                                                         const uint32_t p_frame_count) {
    if (p_src_l == nullptr || p_src_r == nullptr || p_dst == nullptr) {
        return;
    }

    for (uint32_t frame_index = 0; frame_index < p_frame_count; frame_index++) {
        p_dst[frame_index * 2] = p_src_l[frame_index];
        p_dst[frame_index * 2 + 1] = p_src_r[frame_index];
    }
}

void Lowl::Audio::Simd::interleave_stereo_int16_scalar(const float *LOWL_RESTRICT p_src_l,
                                                       const float *LOWL_RESTRICT p_src_r,
                                                       int16_t *LOWL_RESTRICT p_dst,
                                                       const uint32_t p_frame_count) {
    if (p_src_l == nullptr || p_src_r == nullptr || p_dst == nullptr) {
        return;
    }

    for (uint32_t frame_index = 0; frame_index < p_frame_count; frame_index++) {
        const float left = std::clamp(p_src_l[frame_index], -1.0f, 1.0f);
        const float right = std::clamp(p_src_r[frame_index], -1.0f, 1.0f);
        p_dst[frame_index * 2] = static_cast<int16_t>(left * 32767.0f);
        p_dst[frame_index * 2 + 1] = static_cast<int16_t>(right * 32767.0f);
    }
}

Lowl::Audio::Simd::Dispatch Lowl::Audio::Simd::make_scalar_dispatch() {
    Dispatch dispatch{};
    dispatch.mix_scaled_channel = mix_scaled_channel_scalar;
    dispatch.interleave_stereo_float32 = interleave_stereo_float32_scalar;
    dispatch.interleave_stereo_int16 = interleave_stereo_int16_scalar;
    return dispatch;
}
