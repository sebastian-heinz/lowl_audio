#ifndef LOWL_SAMPLE_CONVERTER_H
#define LOWL_SAMPLE_CONVERTER_H

#include <cstdint>

namespace Lowl {
    class SampleConverter {
    public:
        virtual float to_float(int32_t p_sample) const;

        virtual float to_float(int16_t p_sample) const;

        virtual float to_float(int8_t p_sample) const;

        virtual float to_float(uint8_t p_sample) const;

        virtual ~SampleConverter() = default;
    };
}
#endif
