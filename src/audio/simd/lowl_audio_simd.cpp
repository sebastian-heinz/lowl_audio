#include "audio/simd/lowl_audio_simd.h"

#if !defined(LOWL_TYPE_SAMPLE_64) && defined(__aarch64__)
#define LOWL_AUDIO_HAVE_NEON_SIMD 1
namespace Lowl::Audio::Simd {
    void mix_scaled_channel_neon(const Sample *LOWL_RESTRICT p_src,
                                 Sample *LOWL_RESTRICT p_dst,
                                 Sample p_gain,
                                 uint32_t p_frame_count);
    void interleave_stereo_float32_neon(const Sample *LOWL_RESTRICT p_src_l,
                                        const Sample *LOWL_RESTRICT p_src_r,
                                        float *LOWL_RESTRICT p_dst,
                                        uint32_t p_frame_count);
    void interleave_stereo_int16_neon(const Sample *LOWL_RESTRICT p_src_l,
                                      const Sample *LOWL_RESTRICT p_src_r,
                                      int16_t *LOWL_RESTRICT p_dst,
                                      uint32_t p_frame_count);
}
#else
#define LOWL_AUDIO_HAVE_NEON_SIMD 0
#endif

#if !defined(LOWL_TYPE_SAMPLE_64) && (defined(__x86_64__) || defined(_M_X64))
#define LOWL_AUDIO_HAVE_AVX2_SIMD 1
namespace Lowl::Audio::Simd {
    void mix_scaled_channel_avx2(const Sample *LOWL_RESTRICT p_src,
                                 Sample *LOWL_RESTRICT p_dst,
                                 Sample p_gain,
                                 uint32_t p_frame_count);
    void interleave_stereo_float32_avx2(const Sample *LOWL_RESTRICT p_src_l,
                                        const Sample *LOWL_RESTRICT p_src_r,
                                        float *LOWL_RESTRICT p_dst,
                                        uint32_t p_frame_count);
    void interleave_stereo_int16_avx2(const Sample *LOWL_RESTRICT p_src_l,
                                      const Sample *LOWL_RESTRICT p_src_r,
                                      int16_t *LOWL_RESTRICT p_dst,
                                      uint32_t p_frame_count);
}
#else
#define LOWL_AUDIO_HAVE_AVX2_SIMD 0
#endif

namespace {
    Lowl::Audio::Simd::Dispatch make_dispatch() {
        Lowl::Audio::Simd::Dispatch dispatch = Lowl::Audio::Simd::make_scalar_dispatch();

#if LOWL_AUDIO_HAVE_NEON_SIMD
        dispatch.mix_scaled_channel = Lowl::Audio::Simd::mix_scaled_channel_neon;
        dispatch.interleave_stereo_float32 = Lowl::Audio::Simd::interleave_stereo_float32_neon;
        dispatch.interleave_stereo_int16 = Lowl::Audio::Simd::interleave_stereo_int16_neon;
#elif LOWL_AUDIO_HAVE_AVX2_SIMD
#if defined(__GNUC__) || defined(__clang__)
        if (__builtin_cpu_supports("avx2") && __builtin_cpu_supports("fma")) {
            dispatch.mix_scaled_channel = Lowl::Audio::Simd::mix_scaled_channel_avx2;
            dispatch.interleave_stereo_float32 = Lowl::Audio::Simd::interleave_stereo_float32_avx2;
            dispatch.interleave_stereo_int16 = Lowl::Audio::Simd::interleave_stereo_int16_avx2;
        }
#endif
#endif

        return dispatch;
    }
} // namespace

const Lowl::Audio::Simd::Dispatch &Lowl::Audio::Simd::dispatch() {
    static const Dispatch dispatch_table = make_dispatch();
    return dispatch_table;
}
