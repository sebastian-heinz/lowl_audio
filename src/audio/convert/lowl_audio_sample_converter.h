#ifndef LOWL_SAMPLE_CONVERTER_H
#define LOWL_SAMPLE_CONVERTER_H

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>

#include "audio/lowl_audio_sample_format.h"
#include "lowl_typedef.h"

namespace Lowl::Audio {
    class SampleConverter {

    private:
        SampleConverter() {
            // Disallow creating an instance of this object
        };

    public:
        static LOWL_INLINE float uint8_to_float(uint8_t p_sample) {
            if (p_sample >= 128) {
                return static_cast<float>(static_cast<int>(p_sample) - 128) / 127.0f;
            }
            return static_cast<float>(static_cast<int>(p_sample) - 128) / 128.0f;
        }

        static LOWL_INLINE float int8_to_float(int8_t p_sample) {
            if (p_sample > 0) {
                return static_cast<float>(p_sample) / 0x7F;
            } else {
                return static_cast<float>(p_sample) / 0x80;
            }
        }

        static LOWL_INLINE float int16_to_float(int16_t p_sample) {
            if (p_sample > 0) {
                return static_cast<float>(p_sample) / 0x7FFF;
            } else {
                return static_cast<float>(p_sample) / 0x8000;
            }
        }

        static LOWL_INLINE float int32_to_float(int32_t p_sample) {
            if (p_sample > 0) {
                const float positive_limit = static_cast<float>(std::numeric_limits<int32_t>::max());
                return static_cast<float>(p_sample) / positive_limit;
            } else {
                const float negative_magnitude = -static_cast<float>(std::numeric_limits<int32_t>::min());
                return static_cast<float>(p_sample) / negative_magnitude;
            }
        }

        static LOWL_INLINE int32_t sample_to_int24(Lowl::Sample p_sample) {
            const double clamped = std::clamp(static_cast<double>(p_sample), -1.0, 1.0);
            return static_cast<int32_t>(std::lround(clamped * 0x7FFFFF));
        }

        static LOWL_INLINE int32_t sample_to_int32(Lowl::Sample p_sample) {
            const double clamped = std::clamp(static_cast<double>(p_sample), -1.0, 1.0);
            const double scaled = clamped * 0x7FFFFFFF;
            return static_cast<int32_t>(scaled);
        }

        static LOWL_INLINE int16_t sample_to_int16(Lowl::Sample p_sample) {
            const float clamped = std::clamp(static_cast<float>(p_sample), -1.0f, 1.0f);
            return static_cast<int16_t>(clamped * 32767.0f);
        }

        static LOWL_INLINE float sample_to_float(Lowl::Sample p_sample) {
            return static_cast<float>(p_sample);
        }

        static LOWL_INLINE double sample_to_float64(Lowl::Sample p_sample) {
            return static_cast<double>(p_sample);
        }

        static LOWL_INLINE uint8_t sample_to_uint8(Lowl::Sample p_sample) {
            float clamped = std::clamp(static_cast<float>(p_sample), -1.0f, 1.0f);
            return static_cast<uint8_t>(static_cast<int>(clamped * 127.0f) + 128);
        }

        static LOWL_INLINE int8_t sample_to_int8(Lowl::Sample p_sample) {
            const float clamped = std::clamp(static_cast<float>(p_sample), -1.0f, 1.0f);
            const int8_t int8_value = static_cast<int8_t>(clamped * 127.0f);
            return int8_value;
        }

        static LOWL_INLINE bool
        write_sample(Lowl::Audio::SampleFormat p_sample_format, Lowl::Sample p_sample, void **p_dst) {
            {
                switch (p_sample_format) {
                    case SampleFormat::INT_16: {
                        const int16_t sample = sample_to_int16(p_sample);
                        int16_t *dst = static_cast<int16_t *>(*p_dst);
                        *dst++ = sample;
                        *p_dst = dst;
                        return true;
                    }
                    case SampleFormat::INT_24: {
                        const int32_t sample = sample_to_int24(p_sample);
                        uint8_t *dst = static_cast<uint8_t *>(*p_dst);
                        *dst++ = static_cast<uint8_t>(sample);        // bits 0-7
                        *dst++ = static_cast<uint8_t>(sample >> 8);   // bits 8-15
                        *dst++ = static_cast<uint8_t>(sample >> 16);  // bits 16-23
                        *p_dst = dst;
                        return true;
                    }
                    case SampleFormat::INT_32: {
                        const int32_t sample = sample_to_int32(p_sample);
                        int32_t *dst = static_cast<int32_t *>(*p_dst);
                        *dst++ = sample;
                        *p_dst = dst;
                        return true;
                    }
                    case SampleFormat::FLOAT_32: {
                        const float sample = sample_to_float(p_sample);
                        float *dst = static_cast<float *>(*p_dst);
                        *dst++ = sample;
                        *p_dst = dst;
                        return true;
                    }
                    case SampleFormat::FLOAT_64: {
                        const double sample = sample_to_float64(p_sample);
                        double *dst = static_cast<double *>(*p_dst);
                        *dst++ = sample;
                        *p_dst = dst;
                        return true;
                    }
                    case SampleFormat::U_INT_8: {
                        const uint8_t sample = sample_to_uint8(p_sample);
                        uint8_t *dst = static_cast<uint8_t *>(*p_dst);
                        *dst++ = sample;
                        *p_dst = dst;
                        return true;
                    }
                    case SampleFormat::Unknown:
                        return false;
                    case SampleFormat::INT_8: {
                        const int8_t sample = sample_to_int8(p_sample);
                        int8_t *dst = static_cast<int8_t *>(*p_dst);
                        *dst++ = sample;
                        *p_dst = dst;
                        return true;
                    }
                }
            }
            return false;
        }
    };
} // namespace Lowl::Audio
#endif
