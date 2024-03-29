#ifndef LOWL_SAMPLE_FORMAT
#define LOWL_SAMPLE_FORMAT

#include <cstddef>
#include <string>

namespace Lowl::Audio {

    enum class SampleFormat {
        Unknown = 0,
        FLOAT_32 = 1,
        FLOAT_64 = 2,
        INT_32 = 3,
        INT_24 = 4,
        INT_16 = 5,
        INT_8 = 6,
        U_INT_8 = 7,
    };

    inline std::string sample_format_to_string(SampleFormat e) noexcept {
        switch (e) {
            case SampleFormat::Unknown:
                return "Unknown";
            case SampleFormat::FLOAT_32:
                return "FLOAT_32";
            case SampleFormat::FLOAT_64:
                return "FLOAT_64";
            case SampleFormat::INT_32:
                return "INT_32";
            case SampleFormat::INT_24:
                return "INT_24";
            case SampleFormat::INT_16:
                return "INT_16";
            case SampleFormat::INT_8:
                return "INT_8";
            case SampleFormat::U_INT_8:
                return "U_INT_8";
            default:
                return "Undefined";
        }
    }


    inline size_t get_sample_size(SampleFormat format) {
        switch (format) {
            case SampleFormat::FLOAT_64:
                return 8;
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
            default:
                return 0;
        }
    }

    inline size_t get_sample_bits(SampleFormat format) {
        switch (format) {
            case SampleFormat::FLOAT_64:
                return 8 * 8;
            case SampleFormat::FLOAT_32:
            case SampleFormat::INT_32:
                return 4 * 8;
            case SampleFormat::INT_24:
                return 3 * 8;
            case SampleFormat::INT_16:
                return 2 * 8;
            case SampleFormat::INT_8:
            case SampleFormat::U_INT_8:
                return 1 * 8;
            case SampleFormat::Unknown:
                return 0;
            default:
                return 0;
        }
    }

    inline size_t samples_to_bytes(size_t samples, SampleFormat sample_format) {
        return samples * get_sample_size(sample_format);
    }
}

#endif