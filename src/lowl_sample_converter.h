#ifndef LOWL_SAMPLE_CONVERTER_H
#define LOWL_SAMPLE_CONVERTER_H

#include "lowl_audio_frame.h"
#include "lowl_sample_format.h"
#include "lowl_channel.h"
#include "lowl_audio_format.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace Lowl {
    class SampleConverter {
    public:
        virtual float to_float(int32_t p_sample);

        virtual float to_float(int16_t p_sample);

        virtual float to_float(int8_t p_sample);

        virtual float to_float(uint8_t p_sample);

        virtual ~SampleConverter() = default;
    };
}
#endif
