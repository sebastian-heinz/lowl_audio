#ifndef LOWL_AUDIO_SIMD_H
#define LOWL_AUDIO_SIMD_H

#include <cstdint>

#include "lowl_typedef.h"

namespace Lowl::Audio::Simd {
    using MixScaledChannelFn = void (*)(const Sample *LOWL_RESTRICT p_src,
                                        Sample *LOWL_RESTRICT p_dst,
                                        Sample p_gain,
                                        uint32_t p_frame_count);

    using InterleaveStereoFloat32Fn = void (*)(const float *LOWL_RESTRICT p_src_l,
                                               const float *LOWL_RESTRICT p_src_r,
                                               float *LOWL_RESTRICT p_dst,
                                               uint32_t p_frame_count);

    using InterleaveStereoInt16Fn = void (*)(const float *LOWL_RESTRICT p_src_l,
                                             const float *LOWL_RESTRICT p_src_r,
                                             int16_t *LOWL_RESTRICT p_dst,
                                             uint32_t p_frame_count);

    struct Dispatch {
        MixScaledChannelFn mix_scaled_channel = nullptr;
        InterleaveStereoFloat32Fn interleave_stereo_float32 = nullptr;
        InterleaveStereoInt16Fn interleave_stereo_int16 = nullptr;
    };

    const Dispatch &dispatch();

    void mix_scaled_channel_scalar(const Sample *LOWL_RESTRICT p_src,
                                   Sample *LOWL_RESTRICT p_dst,
                                   Sample p_gain,
                                   uint32_t p_frame_count);

    void interleave_stereo_float32_scalar(const float *LOWL_RESTRICT p_src_l,
                                          const float *LOWL_RESTRICT p_src_r,
                                          float *LOWL_RESTRICT p_dst,
                                          uint32_t p_frame_count);

    void interleave_stereo_int16_scalar(const float *LOWL_RESTRICT p_src_l,
                                        const float *LOWL_RESTRICT p_src_r,
                                        int16_t *LOWL_RESTRICT p_dst,
                                        uint32_t p_frame_count);

    Dispatch make_scalar_dispatch();
} // namespace Lowl::Audio::Simd

#endif
