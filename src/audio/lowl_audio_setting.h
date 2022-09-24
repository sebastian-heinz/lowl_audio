#ifndef LOWL_SETTING_H
#define LOWL_SETTING_H


#include "audio/lowl_audio_sample_format.h"

#include <vector>

namespace Lowl::Audio {

    class AudioSetting {

    private:
        AudioSetting() {
            // Disallow creating an instance of this object
        }

        static std::vector<double> test_sample_rates;
        static std::vector<SampleFormat> test_sample_formats;

    public:

        static std::vector<double> get_test_sample_rates();

        static std::vector<SampleFormat> get_test_sample_formats();

    };
}
#endif /* LOWL_SETTING_H */