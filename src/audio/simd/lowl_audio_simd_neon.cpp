#include "audio/simd/lowl_audio_simd.h"

#if !defined(LOWL_TYPE_SAMPLE_64) && defined(__aarch64__)

#include <algorithm>

#include <arm_neon.h>

namespace Lowl::Audio::Simd {
void mix_scaled_channel_neon(const Sample *LOWL_RESTRICT p_src,
                             Sample *LOWL_RESTRICT p_dst,
                             const Sample p_gain,
                             const uint32_t p_frame_count) {
    if (p_src == nullptr || p_dst == nullptr) {
        return;
    }

    const float32x4_t gain = vdupq_n_f32(p_gain);
    uint32_t frame_index = 0;
    for (; frame_index + 4 <= p_frame_count; frame_index += 4) {
        const float32x4_t src = vld1q_f32(p_src + frame_index);
        const float32x4_t dst = vld1q_f32(p_dst + frame_index);
        vst1q_f32(p_dst + frame_index, vfmaq_f32(dst, src, gain));
    }

    for (; frame_index < p_frame_count; frame_index++) {
        p_dst[frame_index] += p_src[frame_index] * p_gain;
    }
}

void interleave_stereo_float32_neon(const float *LOWL_RESTRICT p_src_l,
                                    const float *LOWL_RESTRICT p_src_r,
                                    float *LOWL_RESTRICT p_dst,
                                    const uint32_t p_frame_count) {
    if (p_src_l == nullptr || p_src_r == nullptr || p_dst == nullptr) {
        return;
    }

    uint32_t frame_index = 0;
    for (; frame_index + 4 <= p_frame_count; frame_index += 4) {
        float32x4x2_t lanes{};
        lanes.val[0] = vld1q_f32(p_src_l + frame_index);
        lanes.val[1] = vld1q_f32(p_src_r + frame_index);
        vst2q_f32(p_dst + frame_index * 2, lanes);
    }

    for (; frame_index < p_frame_count; frame_index++) {
        p_dst[frame_index * 2] = p_src_l[frame_index];
        p_dst[frame_index * 2 + 1] = p_src_r[frame_index];
    }
}

void interleave_stereo_int16_neon(const float *LOWL_RESTRICT p_src_l,
                                  const float *LOWL_RESTRICT p_src_r,
                                  int16_t *LOWL_RESTRICT p_dst,
                                  const uint32_t p_frame_count) {
    if (p_src_l == nullptr || p_src_r == nullptr || p_dst == nullptr) {
        return;
    }

    const float32x4_t min_value = vdupq_n_f32(-1.0f);
    const float32x4_t max_value = vdupq_n_f32(1.0f);
    const float32x4_t scale = vdupq_n_f32(32767.0f);

    uint32_t frame_index = 0;
    for (; frame_index + 4 <= p_frame_count; frame_index += 4) {
        float32x4_t left = vld1q_f32(p_src_l + frame_index);
        float32x4_t right = vld1q_f32(p_src_r + frame_index);

        left = vmulq_f32(vminq_f32(vmaxq_f32(left, min_value), max_value), scale);
        right = vmulq_f32(vminq_f32(vmaxq_f32(right, min_value), max_value), scale);

        const int32x4_t left_i32 = vcvtq_s32_f32(left);
        const int32x4_t right_i32 = vcvtq_s32_f32(right);
        const int32x4x2_t interleaved = vzipq_s32(left_i32, right_i32);
        const int16x4_t low = vqmovn_s32(interleaved.val[0]);
        const int16x4_t high = vqmovn_s32(interleaved.val[1]);
        vst1q_s16(p_dst + frame_index * 2, vcombine_s16(low, high));
    }

    for (; frame_index < p_frame_count; frame_index++) {
        const float left = std::min(std::max(p_src_l[frame_index], -1.0f), 1.0f);
        const float right = std::min(std::max(p_src_r[frame_index], -1.0f), 1.0f);
        p_dst[frame_index * 2] = static_cast<int16_t>(left * 32767.0f);
        p_dst[frame_index * 2 + 1] = static_cast<int16_t>(right * 32767.0f);
    }
}
} // namespace Lowl::Audio::Simd

#endif
