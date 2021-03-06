# include "lowl_sample_converter.h"

float Lowl::SampleConverter::to_float(uint8_t p_sample) const {
    return 0;
}

float Lowl::SampleConverter::to_float(int8_t p_sample) const {
    if (p_sample > 0) {
        return static_cast<float>(p_sample) / 0x7F;
    } else {
        return static_cast<float>(p_sample) / 0x80;
    }
}

float Lowl::SampleConverter::to_float(int16_t p_sample) const {
    if (p_sample > 0) {
        return static_cast<float>(p_sample) / 0x7FFF;
    } else {
        return static_cast<float>(p_sample) / 0x8000;
    }
}

float Lowl::SampleConverter::to_float(int32_t p_sample) const {
    if (p_sample > 0) {
        return static_cast<float>(p_sample) / 0x7FFFFFFF;
    } else {
        return static_cast<float>(p_sample) / 0x80000000;
    }
}