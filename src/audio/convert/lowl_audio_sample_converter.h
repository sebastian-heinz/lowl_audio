#ifndef LOWL_SAMPLE_CONVERTER_H
#define LOWL_SAMPLE_CONVERTER_H

#include "lowl_typedef.h"
#include "audio/lowl_audio_sample_format.h"

#include <cstdint>

namespace Lowl::Audio {
    class SampleConverter {

    public:
        void write_sample(Lowl::Audio::SampleFormat p_sample_format, Lowl::Sample p_sample, void **p_dst);


        virtual float int32_to_float(int32_t p_sample) const;

        virtual float int16_to_float(int16_t p_sample) const;

        virtual float int8_to_float(int8_t p_sample) const;

        virtual float uint8_to_float(uint8_t p_sample) const;


        virtual int32_t sample_to_int24(Lowl::Sample p_sample) const;

        virtual int32_t sample_to_int32(Lowl::Sample p_sample) const;

        virtual int16_t sample_to_int16(Lowl::Sample p_sample) const;

        virtual float sample_to_float(Lowl::Sample p_sample) const;

        virtual ~SampleConverter() = default;

        uint8_t sample_to_uint8(Sample sample);
    };
}
#endif
