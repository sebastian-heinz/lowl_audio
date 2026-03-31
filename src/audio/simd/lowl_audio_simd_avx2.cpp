#include "audio/simd/lowl_audio_simd.h"

#if !defined(LOWL_TYPE_SAMPLE_64) && (defined(__x86_64__) || defined(_M_X64))

#include <algorithm>

#include <immintrin.h>

namespace Lowl::Audio::Simd {
void mix_scaled_channel_avx2(const Sample *LOWL_RESTRICT p_src,
                             Sample *LOWL_RESTRICT p_dst,
                             const Sample p_gain,
                             const uint32_t p_frame_count) {
    if (p_src == nullptr || p_dst == nullptr) {
        return;
    }

    const __m256 gain = _mm256_set1_ps(p_gain);
    uint32_t frame_index = 0;
    for (; frame_index + 8 <= p_frame_count; frame_index += 8) {
        const __m256 src = _mm256_loadu_ps(p_src + frame_index);
        const __m256 dst = _mm256_loadu_ps(p_dst + frame_index);
        _mm256_storeu_ps(p_dst + frame_index, _mm256_fmadd_ps(src, gain, dst));
    }

    for (; frame_index < p_frame_count; frame_index++) {
        p_dst[frame_index] += p_src[frame_index] * p_gain;
    }
}

void interleave_stereo_float32_avx2(const float *LOWL_RESTRICT p_src_l,
                                    const float *LOWL_RESTRICT p_src_r,
                                    float *LOWL_RESTRICT p_dst,
                                    const uint32_t p_frame_count) {
    if (p_src_l == nullptr || p_src_r == nullptr || p_dst == nullptr) {
        return;
    }

    uint32_t frame_index = 0;
    for (; frame_index + 8 <= p_frame_count; frame_index += 8) {
        const __m256 left = _mm256_loadu_ps(p_src_l + frame_index);
        const __m256 right = _mm256_loadu_ps(p_src_r + frame_index);

        const __m256 lo = _mm256_unpacklo_ps(left, right);
        const __m256 hi = _mm256_unpackhi_ps(left, right);

        const __m256 interleaved0 = _mm256_permute2f128_ps(lo, hi, 0x20);
        const __m256 interleaved1 = _mm256_permute2f128_ps(lo, hi, 0x31);

        _mm256_storeu_ps(p_dst + frame_index * 2, interleaved0);
        _mm256_storeu_ps(p_dst + frame_index * 2 + 8, interleaved1);
    }

    for (; frame_index < p_frame_count; frame_index++) {
        p_dst[frame_index * 2] = p_src_l[frame_index];
        p_dst[frame_index * 2 + 1] = p_src_r[frame_index];
    }
}

void interleave_stereo_int16_avx2(const float *LOWL_RESTRICT p_src_l,
                                  const float *LOWL_RESTRICT p_src_r,
                                  int16_t *LOWL_RESTRICT p_dst,
                                  const uint32_t p_frame_count) {
    if (p_src_l == nullptr || p_src_r == nullptr || p_dst == nullptr) {
        return;
    }

    const __m256 min_value = _mm256_set1_ps(-1.0f);
    const __m256 max_value = _mm256_set1_ps(1.0f);
    const __m256 scale = _mm256_set1_ps(32767.0f);

    uint32_t frame_index = 0;
    for (; frame_index + 8 <= p_frame_count; frame_index += 8) {
        __m256 left = _mm256_loadu_ps(p_src_l + frame_index);
        __m256 right = _mm256_loadu_ps(p_src_r + frame_index);

        left = _mm256_mul_ps(_mm256_min_ps(_mm256_max_ps(left, min_value), max_value), scale);
        right = _mm256_mul_ps(_mm256_min_ps(_mm256_max_ps(right, min_value), max_value), scale);

        const __m256i left_i32 = _mm256_cvttps_epi32(left);
        const __m256i right_i32 = _mm256_cvttps_epi32(right);

        const __m128i left_lo = _mm256_castsi256_si128(left_i32);
        const __m128i left_hi = _mm256_extracti128_si256(left_i32, 1);
        const __m128i right_lo = _mm256_castsi256_si128(right_i32);
        const __m128i right_hi = _mm256_extracti128_si256(right_i32, 1);

        const __m128i interleaved_lo0 = _mm_unpacklo_epi32(left_lo, right_lo);
        const __m128i interleaved_lo1 = _mm_unpackhi_epi32(left_lo, right_lo);
        const __m128i interleaved_hi0 = _mm_unpacklo_epi32(left_hi, right_hi);
        const __m128i interleaved_hi1 = _mm_unpackhi_epi32(left_hi, right_hi);

        const __m128i packed_lo = _mm_packs_epi32(interleaved_lo0, interleaved_lo1);
        const __m128i packed_hi = _mm_packs_epi32(interleaved_hi0, interleaved_hi1);

        _mm_storeu_si128(reinterpret_cast<__m128i *>(p_dst + frame_index * 2), packed_lo);
        _mm_storeu_si128(reinterpret_cast<__m128i *>(p_dst + frame_index * 2 + 8), packed_hi);
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
