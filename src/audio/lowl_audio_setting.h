#ifndef LOWL_SETTING_H
#define LOWL_SETTING_H

#include <vector>

#include "audio/lowl_audio_channel.h"
#include "audio/lowl_audio_sample_format.h"

namespace Lowl::Audio {

    class AudioSetting {

    private:
        AudioSetting() {
            // Disallow creating an instance of this object
        }

        static std::vector<double> test_sample_rates;
        static std::vector<SampleFormat> test_sample_formats;
        static std::vector<ChannelLayout> test_channel_layouts;

    public:
        static std::vector<double> get_test_sample_rates();

        static std::vector<SampleFormat> get_test_sample_formats();

        static std::vector<ChannelLayout> get_test_channel_layouts();
    };
} // namespace Lowl::Audio
#endif /* LOWL_SETTING_H */
