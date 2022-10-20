#include "lowl_audio_sample_converter.h"

#include "audio/lowl_audio_sample_format.h"

#include <math.h>

float Lowl::Audio::SampleConverter::uint8_to_float(uint8_t p_sample) const {
    return 0;
}

float Lowl::Audio::SampleConverter::int8_to_float(int8_t p_sample) const {
    if (p_sample > 0) {
        return static_cast<float>(p_sample) / 0x7F;
    } else {
        return static_cast<float>(p_sample) / 0x80;
    }
}

float Lowl::Audio::SampleConverter::int16_to_float(int16_t p_sample) const {
    if (p_sample > 0) {
        return static_cast<float>(p_sample) / 0x7FFF;
    } else {
        return static_cast<float>(p_sample) / 0x8000;
    }
}

float Lowl::Audio::SampleConverter::int32_to_float(int32_t p_sample) const {
    if (p_sample > 0) {
        return static_cast<float>(p_sample) / 0x7FFFFFFF;
    } else {
        return static_cast<float>(p_sample) / 0x80000000;
    }
}

void Lowl::Audio::SampleConverter::write_sample(Lowl::Audio::SampleFormat p_sample_format, Lowl::Sample p_sample,
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
                *dst++ = sample >> 8;
                *dst++ = sample >> 16;
                *dst++ = sample >> 24;
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
        }
    }
}

int32_t Lowl::Audio::SampleConverter::sample_to_int24(Lowl::Sample p_sample) const {
    return lround(p_sample * 0x7FFFFF) & 0xFFFFFF;
}

int32_t Lowl::Audio::SampleConverter::sample_to_int32(Lowl::Sample p_sample) const {
    double scaled = p_sample * 0x7FFFFFFF;
    return (int32_t) scaled;
}

int16_t Lowl::Audio::SampleConverter::sample_to_int16(Lowl::Sample p_sample) const {
    double scaled = (double) (p_sample) * 2147483647.0;
    return (int16_t) scaled;
}

float Lowl::Audio::SampleConverter::sample_to_float(Lowl::Sample p_sample) const {
    return static_cast<float>(p_sample);
}
