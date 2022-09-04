#include "lowl_audio_sample_converter.h"

#include "audio/lowl_audio_sample_format.h"

#include <math.h>

float Lowl::Audio::SampleConverter::to_float(uint8_t p_sample) const {
    return 0;
}

float Lowl::Audio::SampleConverter::to_float(int8_t p_sample) const {
    if (p_sample > 0) {
        return static_cast<float>(p_sample) / 0x7F;
    } else {
        return static_cast<float>(p_sample) / 0x80;
    }
}

float Lowl::Audio::SampleConverter::to_float(int16_t p_sample) const {
    if (p_sample > 0) {
        return static_cast<float>(p_sample) / 0x7FFF;
    } else {
        return static_cast<float>(p_sample) / 0x8000;
    }
}

float Lowl::Audio::SampleConverter::to_float(int32_t p_sample) const {
    if (p_sample > 0) {
        return static_cast<float>(p_sample) / 0x7FFFFFFF;
    } else {
        return static_cast<float>(p_sample) / 0x80000000;
    }
}

int32_t Lowl::Audio::SampleConverter::to_int24(Lowl::Sample p_sample) const {
    return lround(p_sample * 0x7FFFFF) & 0xFFFFFF;
}

int32_t Lowl::Audio::SampleConverter::to_int32(Lowl::Sample p_sample) const {
    return lround(p_sample * 0x80000000) & 0x7FFFFFFF;
}


