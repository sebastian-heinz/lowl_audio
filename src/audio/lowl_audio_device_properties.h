#ifndef LOWL_AUDIO_DEVICE_PROPERTIES_H
#define LOWL_AUDIO_DEVICE_PROPERTIES_H

#include "audio/lowl_audio_source.h"

#include <string>

namespace Lowl::Audio {

    struct AudioDevicePropertiesWasapi {
        uint16_t valid_bits_per_sample;
    };

    struct AudioDeviceProperties {

        bool is_supported;
        SampleRate sample_rate;
        AudioChannel channel;
        SampleFormat sample_format;
        AudioChannelMask channel_map;
        bool exclusive_mode;
        union {
            AudioDevicePropertiesWasapi wasapi;
        };

        std::string to_string() {
            return "{channel:" + std::to_string(get_channel_num(channel)) + "," +
                   "sample_rate:" + std::to_string(sample_rate) + "," +
                   "sample_format:" + sample_format_to_string(sample_format) + "," +
                   "channel_map:" + audio_channel_mask_string(channel_map) + "," +
                   "is_supported:" + std::to_string(is_supported) + "," +
                   "exclusive_mode:" + std::to_string(exclusive_mode) + "}";
        }

        bool operator==(const AudioDeviceProperties &rhs) const {
            return is_supported == rhs.is_supported &&
                   sample_rate == rhs.sample_rate &&
                   channel == rhs.channel &&
                   sample_format == rhs.sample_format &&
                   channel_map == rhs.channel_map &&
                   exclusive_mode == rhs.exclusive_mode;
        }

        bool operator!=(const AudioDeviceProperties &rhs) const {
            return !(rhs == *this);
        }

        bool operator<(const AudioDeviceProperties &rhs) const {
            if (is_supported < rhs.is_supported) {
                return true;
            }
            if (rhs.is_supported < is_supported) {
                return false;
            }
            if (sample_rate < rhs.sample_rate) {
                return true;
            }
            if (rhs.sample_rate < sample_rate) {
                return false;
            }
            if (channel < rhs.channel) {
                return true;
            }
            if (rhs.channel < channel) {
                return false;
            }
            if (sample_format < rhs.sample_format) {
                return true;
            }
            if (rhs.sample_format < sample_format) {
                return false;
            }
            if (channel_map < rhs.channel_map) {
                return true;
            }
            if (rhs.channel_map < channel_map) {
                return false;
            }
            return exclusive_mode < rhs.exclusive_mode;
        }

        bool operator>(const AudioDeviceProperties &rhs) const {
            return rhs < *this;
        }

        bool operator<=(const AudioDeviceProperties &rhs) const {
            return !(rhs < *this);
        }

        bool operator>=(const AudioDeviceProperties &rhs) const {
            return !(*this < rhs);
        }

    };
}

#endif
