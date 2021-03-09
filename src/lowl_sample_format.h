#ifndef LOWL_SAMPLE_FORMAT
#define LOWL_SAMPLE_FORMAT

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

    inline int get_sample_size(Lowl::SampleFormat format) {
        switch (format) {
            case Lowl::SampleFormat::FLOAT_32:
            case Lowl::SampleFormat::INT_32:
                return 4;
            case Lowl::SampleFormat::INT_24:
                return 3;
            case Lowl::SampleFormat::INT_16:
                return 2;
            case Lowl::SampleFormat::INT_8:
            case Lowl::SampleFormat::U_INT_8:
                return 1;
            case Lowl::SampleFormat::Unknown:
                return 0;
        }
    }
}

#endif