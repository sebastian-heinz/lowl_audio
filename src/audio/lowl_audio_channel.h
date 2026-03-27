#ifndef LOWL_AUDIO_CHANNEL_H
#define LOWL_AUDIO_CHANNEL_H

#include <cstdint>
#include <string>

namespace Lowl::Audio {
    enum class Speaker : uint32_t {
        FrontLeft = 1u << 0u,
        FrontRight = 1u << 1u,
        FrontCenter = 1u << 2u,
        LowFrequency = 1u << 3u,
        BackLeft = 1u << 4u,
        BackRight = 1u << 5u,
        FrontLeftOfCenter = 1u << 6u,
        FrontRightOfCenter = 1u << 7u,
        BackCenter = 1u << 8u,
        SideLeft = 1u << 9u,
        SideRight = 1u << 10u,
        TopCenter = 1u << 11u,
        TopFrontLeft = 1u << 12u,
        TopFrontCenter = 1u << 13u,
        TopFrontRight = 1u << 14u,
        TopBackLeft = 1u << 15u,
        TopBackCenter = 1u << 16u,
        TopBackRight = 1u << 17u,
    };

    constexpr uint32_t speaker_bits(const Speaker p_speaker) {
        return static_cast<uint32_t>(p_speaker);
    }

    constexpr uint32_t operator|(const Speaker p_left, const Speaker p_right) {
        return speaker_bits(p_left) | speaker_bits(p_right);
    }

    constexpr uint32_t operator|(const uint32_t p_mask, const Speaker p_speaker) {
        return p_mask | speaker_bits(p_speaker);
    }

    inline const char *speaker_to_string(const Speaker p_speaker) {
        switch (p_speaker) {
            case Speaker::FrontLeft:
                return "FrontLeft";
            case Speaker::FrontRight:
                return "FrontRight";
            case Speaker::FrontCenter:
                return "FrontCenter";
            case Speaker::LowFrequency:
                return "LowFrequency";
            case Speaker::BackLeft:
                return "BackLeft";
            case Speaker::BackRight:
                return "BackRight";
            case Speaker::FrontLeftOfCenter:
                return "FrontLeftOfCenter";
            case Speaker::FrontRightOfCenter:
                return "FrontRightOfCenter";
            case Speaker::BackCenter:
                return "BackCenter";
            case Speaker::SideLeft:
                return "SideLeft";
            case Speaker::SideRight:
                return "SideRight";
            case Speaker::TopCenter:
                return "TopCenter";
            case Speaker::TopFrontLeft:
                return "TopFrontLeft";
            case Speaker::TopFrontCenter:
                return "TopFrontCenter";
            case Speaker::TopFrontRight:
                return "TopFrontRight";
            case Speaker::TopBackLeft:
                return "TopBackLeft";
            case Speaker::TopBackCenter:
                return "TopBackCenter";
            case Speaker::TopBackRight:
                return "TopBackRight";
            default:
                return "None";
        }
    }

    struct ChannelLayout {
        static constexpr uint8_t MaxChannels = 8;

        uint32_t speaker_mask = 0;
        uint8_t channel_count = 0;

        static constexpr uint8_t popcount32(uint32_t x) {
            x = x - ((x >> 1u) & 0x55555555u);
            x = (x & 0x33333333u) + ((x >> 2u) & 0x33333333u);
            return static_cast<uint8_t>((((x + (x >> 4u)) & 0x0F0F0F0Fu) * 0x01010101u) >> 24u);
        }

        static constexpr ChannelLayout from_mask(const uint32_t p_mask) {
            const uint8_t count = popcount32(p_mask);
            return (p_mask == 0u || count == 0u || count > MaxChannels) ? ChannelLayout{} : ChannelLayout{p_mask, count};
        }

        static constexpr ChannelLayout from_count(const uint8_t p_count) {
            switch (p_count) {
                case 1:
                    return Mono;
                case 2:
                    return Stereo;
                case 3:
                    return Surround_3_0;
                case 4:
                    return Quad;
                case 5:
                    return Surround_5_0;
                case 6:
                    return Surround_5_1;
                case 7:
                    return Surround_6_1;
                case 8:
                    return Surround_7_1;
                default:
                    return {};
            }
        }

        constexpr Speaker speaker_at(const uint8_t p_channel_index) const {
            if (p_channel_index >= channel_count) {
                return static_cast<Speaker>(0);
            }

            uint8_t current_index = 0;
            for (uint8_t bit_index = 0; bit_index < 32; bit_index++) {
                const uint32_t bit = 1u << bit_index;
                if ((speaker_mask & bit) == 0u) {
                    continue;
                }
                if (current_index == p_channel_index) {
                    return static_cast<Speaker>(bit);
                }
                current_index++;
            }

            return static_cast<Speaker>(0);
        }

        constexpr int index_of(const Speaker p_speaker) const {
            if (!has(p_speaker)) {
                return -1;
            }

            const uint32_t bit = speaker_bits(p_speaker);
            int current_index = 0;
            for (uint8_t bit_index = 0; bit_index < 32; bit_index++) {
                const uint32_t current_bit = 1u << bit_index;
                if ((speaker_mask & current_bit) == 0u) {
                    continue;
                }
                if (current_bit == bit) {
                    return current_index;
                }
                current_index++;
            }
            return -1;
        }

        constexpr bool has(const Speaker p_speaker) const {
            return (speaker_mask & speaker_bits(p_speaker)) != 0u;
        }

        constexpr bool is_valid() const {
            return speaker_mask != 0u && channel_count > 0u && channel_count == popcount32(speaker_mask);
        }

        constexpr bool operator==(const ChannelLayout &p_other) const {
            return speaker_mask == p_other.speaker_mask && channel_count == p_other.channel_count;
        }

        constexpr bool operator!=(const ChannelLayout &p_other) const {
            return !(*this == p_other);
        }

        std::string to_string() const {
            if (!is_valid()) {
                return "Invalid";
            }
            if (*this == Mono) {
                return "Mono";
            }
            if (*this == Stereo) {
                return "Stereo";
            }
            if (*this == Surround_3_0) {
                return "Surround_3_0";
            }
            if (*this == Quad) {
                return "Quad";
            }
            if (*this == Quad_Side) {
                return "Quad_Side";
            }
            if (*this == Surround_4_0) {
                return "Surround_4_0";
            }
            if (*this == Surround_5_0) {
                return "Surround_5_0";
            }
            if (*this == Surround_5_0_Rear) {
                return "Surround_5_0_Rear";
            }
            if (*this == Surround_5_1) {
                return "Surround_5_1";
            }
            if (*this == Surround_5_1_Rear) {
                return "Surround_5_1_Rear";
            }
            if (*this == Surround_6_1) {
                return "Surround_6_1";
            }
            if (*this == Surround_7_1) {
                return "Surround_7_1";
            }
            if (*this == Surround_7_1_Front) {
                return "Surround_7_1_Front";
            }

            std::string response;
            for (uint8_t channel_index = 0; channel_index < channel_count; channel_index++) {
                if (!response.empty()) {
                    response += ", ";
                }
                response += speaker_to_string(speaker_at(channel_index));
            }
            return response;
        }

        static const ChannelLayout Mono;
        static const ChannelLayout Stereo;
        static const ChannelLayout Surround_3_0;
        static const ChannelLayout Quad;
        static const ChannelLayout Quad_Side;
        static const ChannelLayout Surround_4_0;
        static const ChannelLayout Surround_5_0;
        static const ChannelLayout Surround_5_0_Rear;
        static const ChannelLayout Surround_5_1;
        static const ChannelLayout Surround_5_1_Rear;
        static const ChannelLayout Surround_6_1;
        static const ChannelLayout Surround_7_1;
        static const ChannelLayout Surround_7_1_Front;
    };

    inline constexpr ChannelLayout ChannelLayout::Mono =
        ChannelLayout::from_mask(speaker_bits(Speaker::FrontCenter));
    inline constexpr ChannelLayout ChannelLayout::Stereo =
        ChannelLayout::from_mask(Speaker::FrontLeft | Speaker::FrontRight);
    inline constexpr ChannelLayout ChannelLayout::Surround_3_0 =
        ChannelLayout::from_mask(Speaker::FrontLeft | Speaker::FrontRight | Speaker::FrontCenter);
    inline constexpr ChannelLayout ChannelLayout::Quad =
        ChannelLayout::from_mask(Speaker::FrontLeft | Speaker::FrontRight | Speaker::BackLeft | Speaker::BackRight);
    inline constexpr ChannelLayout ChannelLayout::Quad_Side =
        ChannelLayout::from_mask(Speaker::FrontLeft | Speaker::FrontRight | Speaker::SideLeft | Speaker::SideRight);
    inline constexpr ChannelLayout ChannelLayout::Surround_4_0 = ChannelLayout::from_mask(
        Speaker::FrontLeft | Speaker::FrontRight | Speaker::FrontCenter | Speaker::BackCenter);
    inline constexpr ChannelLayout ChannelLayout::Surround_5_0 = ChannelLayout::from_mask(
        Speaker::FrontLeft | Speaker::FrontRight | Speaker::FrontCenter | Speaker::SideLeft | Speaker::SideRight);
    inline constexpr ChannelLayout ChannelLayout::Surround_5_0_Rear = ChannelLayout::from_mask(
        Speaker::FrontLeft | Speaker::FrontRight | Speaker::FrontCenter | Speaker::BackLeft | Speaker::BackRight);
    inline constexpr ChannelLayout ChannelLayout::Surround_5_1 = ChannelLayout::from_mask(
        Speaker::FrontLeft | Speaker::FrontRight | Speaker::FrontCenter | Speaker::LowFrequency | Speaker::SideLeft |
        Speaker::SideRight);
    inline constexpr ChannelLayout ChannelLayout::Surround_5_1_Rear = ChannelLayout::from_mask(
        Speaker::FrontLeft | Speaker::FrontRight | Speaker::FrontCenter | Speaker::LowFrequency | Speaker::BackLeft |
        Speaker::BackRight);
    inline constexpr ChannelLayout ChannelLayout::Surround_6_1 = ChannelLayout::from_mask(
        Speaker::FrontLeft | Speaker::FrontRight | Speaker::FrontCenter | Speaker::LowFrequency | Speaker::BackCenter |
        Speaker::SideLeft | Speaker::SideRight);
    inline constexpr ChannelLayout ChannelLayout::Surround_7_1 = ChannelLayout::from_mask(
        Speaker::FrontLeft | Speaker::FrontRight | Speaker::FrontCenter | Speaker::LowFrequency | Speaker::BackLeft |
        Speaker::BackRight | Speaker::SideLeft | Speaker::SideRight);
    inline constexpr ChannelLayout ChannelLayout::Surround_7_1_Front = ChannelLayout::from_mask(
        Speaker::FrontLeft | Speaker::FrontRight | Speaker::FrontCenter | Speaker::LowFrequency |
        Speaker::FrontLeftOfCenter | Speaker::FrontRightOfCenter | Speaker::SideLeft | Speaker::SideRight);
} // namespace Lowl::Audio

#endif
