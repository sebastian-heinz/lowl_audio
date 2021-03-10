#ifndef LOWL_SAMPLE_FORMAT
#define LOWL_SAMPLE_FORMAT

#include <cstdint>
#include <cstddef>

namespace Lowl {
    enum class SampleFormat {
        Unknown = 11,
        FLOAT_32 = 0,
        INT_32 = 1,
        INT_24 = 2,
        INT_16 = 3,
        INT_8 = 4,
        U_INT_8 = 5,
    };

    inline int get_sample_size(SampleFormat format) {
        switch (format) {
            case SampleFormat::FLOAT_32:
            case SampleFormat::INT_32:
                return 4;
            case SampleFormat::INT_24:
                return 3;
            case SampleFormat::INT_16:
                return 2;
            case SampleFormat::INT_8:
            case SampleFormat::U_INT_8:
                return 1;
            case SampleFormat::Unknown:
                return 0;
        }
    }

    inline float sample_to_float(int16_t sample) {
        if (sample > 0) {
            return sample / 0x7FFF;
        } else {
            return sample / 0x8000;
        }
    }

    inline int16_t sample_to_int16(float sample) {
        if (sample > 0) {
            return sample * 0x7FFF;
        } else {
            return sample * 0x8000;
        }
    }



}

#endif