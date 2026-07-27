#ifndef LOWL_AUDIO_DEVICE_PROPERTIES_H
#define LOWL_AUDIO_DEVICE_PROPERTIES_H

#include <string>

#include "audio/lowl_audio_format.h"
#include "audio/lowl_audio_sample_format.h"

namespace Lowl::Audio {
    struct AudioDevicePropertiesWasapi {
        uint16_t valid_bits_per_sample = 0;
    };

    struct AudioDeviceProperties {
        bool is_supported = false;
        AudioFormat audio_format{};
        SampleFormat sample_format = SampleFormat::Unknown;
        bool exclusive_mode = false;
        AudioDevicePropertiesWasapi wasapi{};

        const AudioFormat &get_audio_format() const {
            return audio_format;
        }

        std::string to_string() const {
            return "{channel_count:" + std::to_string(audio_format.channel_layout.channel_count) + "," +
                   "channel_layout:" + audio_format.channel_layout.to_string() + "," +
                   "sample_rate:" + std::to_string(audio_format.sample_rate) + "," +
                   "sample_format:" + std::string(sample_format_to_string(sample_format)) + "," +
                   "valid_bits_per_sample:" + std::to_string(wasapi.valid_bits_per_sample) + "," +
                   "is_supported:" + std::to_string(is_supported) + "," +
                   "exclusive_mode:" + std::to_string(exclusive_mode) + "}";
        }

        bool operator==(const AudioDeviceProperties &rhs) const {
            return is_supported == rhs.is_supported && audio_format == rhs.audio_format &&
                   sample_format == rhs.sample_format && exclusive_mode == rhs.exclusive_mode &&
                   wasapi.valid_bits_per_sample == rhs.wasapi.valid_bits_per_sample;
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
            const uint32_l lhs_sample_rate = Lowl::Audio::normalize_sample_rate(audio_format.sample_rate);
            const uint32_l rhs_sample_rate = Lowl::Audio::normalize_sample_rate(rhs.audio_format.sample_rate);
            if (lhs_sample_rate < rhs_sample_rate) {
                return true;
            }
            if (rhs_sample_rate < lhs_sample_rate) {
                return false;
            }
            if (audio_format.channel_layout.channel_count < rhs.audio_format.channel_layout.channel_count) {
                return true;
            }
            if (rhs.audio_format.channel_layout.channel_count < audio_format.channel_layout.channel_count) {
                return false;
            }
            if (sample_format < rhs.sample_format) {
                return true;
            }
            if (rhs.sample_format < sample_format) {
                return false;
            }
            if (wasapi.valid_bits_per_sample < rhs.wasapi.valid_bits_per_sample) {
                return true;
            }
            if (rhs.wasapi.valid_bits_per_sample < wasapi.valid_bits_per_sample) {
                return false;
            }
            if (audio_format.channel_layout.speaker_mask < rhs.audio_format.channel_layout.speaker_mask) {
                return true;
            }
            if (rhs.audio_format.channel_layout.speaker_mask < audio_format.channel_layout.speaker_mask) {
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
} // namespace Lowl::Audio

#endif
