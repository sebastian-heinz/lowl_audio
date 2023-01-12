#ifndef LOWL_SAMPLE_CONVERTER_H
#define LOWL_SAMPLE_CONVERTER_H

#include "lowl_typedef.h"

#include "audio/lowl_audio_sample_format.h"

#include <math.h>
#include <cstdint>

namespace Lowl::Audio {
    class SampleConverter {

    private:
        SampleConverter() {
            // Disallow creating an instance of this object
        };

    public:
        static _INLINE_ float uint8_to_float(uint8_t p_sample) {
            return 0;
        }

        static _INLINE_ float int8_to_float(int8_t p_sample) {
            if (p_sample > 0) {
                return static_cast<float>(p_sample) / 0x7F;
            } else {
                return static_cast<float>(p_sample) / 0x80;
            }
        }

        static _INLINE_ float int16_to_float(int16_t p_sample) {
            if (p_sample > 0) {
                return static_cast<float>(p_sample) / 0x7FFF;
            } else {
                return static_cast<float>(p_sample) / 0x8000;
            }
        }

        static _INLINE_ float int32_to_float(int32_t p_sample) {
            if (p_sample > 0) {
                return static_cast<float>(p_sample) / 0x7FFFFFFF;
            } else {
                return static_cast<float>(p_sample) / 0x80000000;
            }
        }

        static _INLINE_ int32_t sample_to_int24(Lowl::Sample p_sample) {
            return lround(p_sample * 0x7FFFFF) & 0xFFFFFF;
        }

        static _INLINE_ int32_t sample_to_int32(Lowl::Sample p_sample) {
            double scaled = p_sample * 0x7FFFFFFF;
            return (int32_t) scaled;
        }

        static _INLINE_ int16_t sample_to_int16(Lowl::Sample p_sample) {
            int16_t int16 = (int16_t) (p_sample * (32767.0f));
            return int16;
        }

        static _INLINE_ float sample_to_float(Lowl::Sample p_sample) {
            return static_cast<float>(p_sample);
        }

        static _INLINE_ uint8_t sample_to_uint8(Lowl::Sample p_sample) {
            uint8_t scaled = (uint8_t) (128 + ((uint8_t) (p_sample * (127.0f))));
            return scaled;
        }

        static _INLINE_ int8_t sample_to_int8(Lowl::Sample p_sample) {
            int8_t int16 = (int8_t) (p_sample * (127.0f));
            return int16;
        }

        static _INLINE_ void write_sample(Lowl::Audio::SampleFormat p_sample_format, Lowl::Sample p_sample,
                                          void **p_dst) {
            {
                switch (p_sample_format) {
                    case SampleFormat::INT_16: {
                        int16_t sample = sample_to_int16(p_sample);
                        int16_t *dst = (int16_t *) *p_dst;
                        *dst++ = sample;
                        *p_dst = dst;
                        break;
                    }
                    case SampleFormat::INT_24: {
                        int32_t sample = sample_to_int24(p_sample);
                        uint8_t *dst = (uint8_t *) *p_dst;
                        *dst++ = static_cast<uint8_t>(sample >> 8);
                        *dst++ = static_cast<uint8_t>(sample >> 16);
                        *dst++ = static_cast<uint8_t>(sample >> 24);
                        *p_dst = dst;
                        break;
                    }
                    case SampleFormat::INT_32: {
                        int32_t sample = sample_to_int32(p_sample);
                        int32_t *dst = (int32_t *) *p_dst;
                        *dst++ = sample;
                        *p_dst = dst;
                        break;
                    }
                    case SampleFormat::FLOAT_32: {
                        float sample = sample_to_float(p_sample);
                        float *dst = (float *) *p_dst;
                        *dst++ = sample;
                        *p_dst = dst;
                        break;
                    }
                    case SampleFormat::U_INT_8: {
                        uint8_t sample = sample_to_uint8(p_sample);
                        uint8_t *dst = (uint8_t *) *p_dst;
                        *dst++ = sample;
                        *p_dst = dst;
                        break;
                    }
                    case SampleFormat::Unknown:
                        break;
                    case SampleFormat::FLOAT_64:
                        break;
                    case SampleFormat::INT_8:
                        int8_t sample = sample_to_int8(p_sample);
                        int8_t *dst = (int8_t *) *p_dst;
                        *dst++ = sample;
                        *p_dst = dst;
                        break;
                }
            }
        }

    };
}
#endif
