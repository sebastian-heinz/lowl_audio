#ifndef LOWL_AUDIO_CHANNEL
#define LOWL_AUDIO_CHANNEL

#include <string>

namespace Lowl::Audio {
    enum class AudioChannel {
        None = 0,
        Mono = 1,
        Stereo = 2,
        Quadraphonic = 4, // Surround
    };

    enum class AudioChannelMask : uint32_t {
        NONE = 0,
        MONO = (1 << 0),
        LEFT = (1 << 1),
        RIGHT = (1 << 2),
        FRONT_LEFT = LEFT,
        FRONT_RIGHT = RIGHT,
        FRONT_CENTER = (1 << 3),
        LOW_FREQUENCY = (1 << 4),
        BACK_LEFT = (1 << 5),
        BACK_RIGHT = (1 << 6),
        FRONT_LEFT_OF_CENTER = (1 << 7),
        FRONT_RIGHT_OF_CENTER = (1 << 8),
        BACK_CENTER = (1 << 9),
        SIDE_LEFT = (1 << 10),
        SIDE_RIGHT = (1 << 11),
        TOP_CENTER = (1 << 12),
        TOP_FRONT_LEFT = (1 << 13),
        TOP_FRONT_CENTER = (1 << 14),
        TOP_FRONT_RIGHT = (1 << 15),
        TOP_BACK_LEFT = (1 << 16),
        TOP_BACK_CENTER = (1 << 17),
        TOP_BACK_RIGHT = (1 << 18)
    };

    constexpr enum AudioChannelMask operator|(const enum AudioChannelMask p_self, const enum AudioChannelMask p_in) {
        return (enum AudioChannelMask) (uint32_t(p_self) | uint32_t(p_in));
    }

    constexpr enum AudioChannelMask operator&(const enum AudioChannelMask p_self, const enum AudioChannelMask p_in) {
        return (enum AudioChannelMask) (uint32_t(p_self) & uint32_t(p_in));
    }

    inline std::string audio_channel_mask_string(AudioChannelMask p_channel_mask) {
        std::string response;
        if ((uint32_t) (p_channel_mask & AudioChannelMask::MONO))
            response += "MONO | ";
        if ((uint32_t) (p_channel_mask & AudioChannelMask::LEFT))
            response += "LEFT | ";
        if ((uint32_t) (p_channel_mask & AudioChannelMask::RIGHT))
            response += "RIGHT | ";
        if ((uint32_t) (p_channel_mask & AudioChannelMask::FRONT_CENTER))
            response += "FRONT_CENTER | ";
        if ((uint32_t) (p_channel_mask & AudioChannelMask::LOW_FREQUENCY))
            response += "LOW_FREQUENCY | ";
        if ((uint32_t) (p_channel_mask & AudioChannelMask::BACK_LEFT))
            response += "BACK_LEFT | ";
        if ((uint32_t) (p_channel_mask & AudioChannelMask::BACK_RIGHT))
            response += "BACK_RIGHT | ";
        if ((uint32_t) (p_channel_mask & AudioChannelMask::FRONT_LEFT_OF_CENTER))
            response += "FRONT_LEFT_OF_CENTER | ";
        if ((uint32_t) (p_channel_mask & AudioChannelMask::FRONT_RIGHT_OF_CENTER))
            response += "FRONT_RIGHT_OF_CENTER | ";
        if ((uint32_t) (p_channel_mask & AudioChannelMask::BACK_CENTER))
            response += "BACK_CENTER | ";
        if ((uint32_t) (p_channel_mask & AudioChannelMask::SIDE_LEFT))
            response += "SIDE_LEFT | ";
        if ((uint32_t) (p_channel_mask & AudioChannelMask::SIDE_RIGHT))
            response += "SIDE_RIGHT | ";
        if ((uint32_t) (p_channel_mask & AudioChannelMask::TOP_CENTER))
            response += "TOP_CENTER | ";
        if ((uint32_t) (p_channel_mask & AudioChannelMask::TOP_FRONT_LEFT))
            response += "TOP_FRONT_LEFT | ";
        if ((uint32_t) (p_channel_mask & AudioChannelMask::TOP_FRONT_CENTER))
            response += "TOP_FRONT_CENTER | ";
        if ((uint32_t) (p_channel_mask & AudioChannelMask::TOP_FRONT_RIGHT))
            response += "TOP_FRONT_RIGHT | ";
        if ((uint32_t) (p_channel_mask & AudioChannelMask::TOP_BACK_LEFT))
            response += "TOP_BACK_LEFT | ";
        if ((uint32_t) (p_channel_mask & AudioChannelMask::TOP_BACK_CENTER))
            response += "TOP_BACK_CENTER | ";
        if ((uint32_t) (p_channel_mask & AudioChannelMask::TOP_BACK_RIGHT))
            response += "TOP_BACK_RIGHT | ";
        if (response.empty()) {
            response = "NONE";
        } else {
            response.erase(response.end() - 3, response.end());
        }
        return response;
    }


    inline size_t get_channel_num(AudioChannel channel) {
        switch (channel) {
            case AudioChannel::None:
                return 0;
            case AudioChannel::Mono:
                return 1;
            case AudioChannel::Stereo:
                return 2;
            case AudioChannel::Quadraphonic:
                return 4;
            default:
                return 0;
        }
    }

    inline AudioChannel get_channel(int channel) {
        switch (channel) {
            case 1:
                return AudioChannel::Mono;
            case 2:
                return AudioChannel::Stereo;
            case 4:
                return AudioChannel::Quadraphonic;
            default:
                return AudioChannel::None;
        }
    }

}

#endif