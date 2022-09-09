#ifndef LOWL_AUDIO_DEVICE_PROPERTIES_H
#define LOWL_AUDIO_DEVICE_PROPERTIES_H

#include "audio/lowl_audio_source.h"

namespace Lowl::Audio {

    struct AudioDevicePropertiesWasapi {
        uint16_t valid_bits_per_sample;
    };

    struct AudioDeviceProperties {
        SampleRate sample_rate;
        AudioChannel channel;
        SampleFormat sample_format;
        bool exclusive_mode;
        union {
            AudioDevicePropertiesWasapi wasapi;
        };
    };
}

#endif
