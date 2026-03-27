#ifdef LOWL_DRIVER_CORE_AUDIO

#include "lowl_audio_core_audio_layout.h"

namespace {
    using Lowl::Audio::AudioChannelMask;
    using Lowl::Audio::AudioDeviceProperties;

    constexpr AudioChannelMask kMonoMask = AudioChannelMask::MONO;
    constexpr AudioChannelMask kStereoMask = AudioChannelMask::LEFT | AudioChannelMask::RIGHT;
    constexpr AudioChannelMask kThreeZeroMask = kStereoMask | AudioChannelMask::FRONT_CENTER;
    constexpr AudioChannelMask kQuadSideMask = kStereoMask | AudioChannelMask::SIDE_LEFT | AudioChannelMask::SIDE_RIGHT;
    constexpr AudioChannelMask kQuadRearMask = kStereoMask | AudioChannelMask::BACK_LEFT | AudioChannelMask::BACK_RIGHT;
    constexpr AudioChannelMask kFourZeroCenterBackMask = kThreeZeroMask | AudioChannelMask::BACK_CENTER;
    constexpr AudioChannelMask kFiveZeroSideMask = kThreeZeroMask | AudioChannelMask::SIDE_LEFT | AudioChannelMask::SIDE_RIGHT;
    constexpr AudioChannelMask kFiveZeroRearMask = kThreeZeroMask | AudioChannelMask::BACK_LEFT | AudioChannelMask::BACK_RIGHT;
    constexpr AudioChannelMask kFiveOneSideMask = kFiveZeroSideMask | AudioChannelMask::LOW_FREQUENCY;
    constexpr AudioChannelMask kFiveOneRearMask = kFiveZeroRearMask | AudioChannelMask::LOW_FREQUENCY;
    constexpr AudioChannelMask kSixOneMask = kFiveOneSideMask | AudioChannelMask::BACK_CENTER;
    constexpr AudioChannelMask kSevenOneFrontMask =
        kFiveOneSideMask | AudioChannelMask::FRONT_LEFT_OF_CENTER | AudioChannelMask::FRONT_RIGHT_OF_CENTER;
    constexpr AudioChannelMask kSevenOneSurroundMask =
        kFiveOneSideMask | AudioChannelMask::BACK_LEFT | AudioChannelMask::BACK_RIGHT;

    AudioChannelMask to_channel_mask_from_label(const AudioChannelLabel p_label) {
        switch (p_label) {
            case kAudioChannelLabel_Mono:
                return AudioChannelMask::MONO;
            case kAudioChannelLabel_Left:
                return AudioChannelMask::LEFT;
            case kAudioChannelLabel_Right:
                return AudioChannelMask::RIGHT;
            case kAudioChannelLabel_Center:
                return AudioChannelMask::FRONT_CENTER;
            case kAudioChannelLabel_LFEScreen:
            case kAudioChannelLabel_LFE2:
            case kAudioChannelLabel_LFE3:
                return AudioChannelMask::LOW_FREQUENCY;
            case kAudioChannelLabel_LeftSurround:
            case kAudioChannelLabel_LeftSurroundDirect:
            case kAudioChannelLabel_LeftSideSurround:
                return AudioChannelMask::SIDE_LEFT;
            case kAudioChannelLabel_RightSurround:
            case kAudioChannelLabel_RightSurroundDirect:
            case kAudioChannelLabel_RightSideSurround:
                return AudioChannelMask::SIDE_RIGHT;
            case kAudioChannelLabel_LeftCenter:
                return AudioChannelMask::FRONT_LEFT_OF_CENTER;
            case kAudioChannelLabel_RightCenter:
                return AudioChannelMask::FRONT_RIGHT_OF_CENTER;
            case kAudioChannelLabel_CenterSurround:
            case kAudioChannelLabel_CenterSurroundDirect:
                return AudioChannelMask::BACK_CENTER;
            case kAudioChannelLabel_RearSurroundLeft:
            case kAudioChannelLabel_LeftBackSurround:
                return AudioChannelMask::BACK_LEFT;
            case kAudioChannelLabel_RearSurroundRight:
            case kAudioChannelLabel_RightBackSurround:
                return AudioChannelMask::BACK_RIGHT;
            case kAudioChannelLabel_TopCenterSurround:
                return AudioChannelMask::TOP_CENTER;
            case kAudioChannelLabel_VerticalHeightLeft:
                return AudioChannelMask::TOP_FRONT_LEFT;
            case kAudioChannelLabel_VerticalHeightCenter:
                return AudioChannelMask::TOP_FRONT_CENTER;
            case kAudioChannelLabel_VerticalHeightRight:
                return AudioChannelMask::TOP_FRONT_RIGHT;
            case kAudioChannelLabel_TopBackLeft:
                return AudioChannelMask::TOP_BACK_LEFT;
            case kAudioChannelLabel_TopBackCenter:
                return AudioChannelMask::TOP_BACK_CENTER;
            case kAudioChannelLabel_TopBackRight:
                return AudioChannelMask::TOP_BACK_RIGHT;
            default:
                return AudioChannelMask::NONE;
        }
    }

    AudioChannelMask to_channel_mask_from_bitmap(const AudioChannelBitmap p_bitmap) {
        struct BitmapMapping {
            AudioChannelBitmap bit;
            AudioChannelMask mask;
        };

        static constexpr BitmapMapping bit_mappings[] = {
            {kAudioChannelBit_Left, AudioChannelMask::LEFT},
            {kAudioChannelBit_Right, AudioChannelMask::RIGHT},
            {kAudioChannelBit_Center, AudioChannelMask::FRONT_CENTER},
            {kAudioChannelBit_LFEScreen, AudioChannelMask::LOW_FREQUENCY},
            {kAudioChannelBit_LeftSurround, AudioChannelMask::SIDE_LEFT},
            {kAudioChannelBit_RightSurround, AudioChannelMask::SIDE_RIGHT},
            {kAudioChannelBit_LeftCenter, AudioChannelMask::FRONT_LEFT_OF_CENTER},
            {kAudioChannelBit_RightCenter, AudioChannelMask::FRONT_RIGHT_OF_CENTER},
            {kAudioChannelBit_CenterSurround, AudioChannelMask::BACK_CENTER},
            {kAudioChannelBit_LeftSurroundDirect, AudioChannelMask::SIDE_LEFT},
            {kAudioChannelBit_RightSurroundDirect, AudioChannelMask::SIDE_RIGHT},
            {kAudioChannelBit_TopCenterSurround, AudioChannelMask::TOP_CENTER},
            {kAudioChannelBit_VerticalHeightLeft, AudioChannelMask::TOP_FRONT_LEFT},
            {kAudioChannelBit_VerticalHeightCenter, AudioChannelMask::TOP_FRONT_CENTER},
            {kAudioChannelBit_VerticalHeightRight, AudioChannelMask::TOP_FRONT_RIGHT},
            {kAudioChannelBit_TopBackLeft, AudioChannelMask::TOP_BACK_LEFT},
            {kAudioChannelBit_TopBackCenter, AudioChannelMask::TOP_BACK_CENTER},
            {kAudioChannelBit_TopBackRight, AudioChannelMask::TOP_BACK_RIGHT},
        };

        uint32_t remaining_bits = static_cast<uint32_t>(p_bitmap);
        AudioChannelMask channel_mask = AudioChannelMask::NONE;
        for (const BitmapMapping &mapping : bit_mappings) {
            const uint32_t bit = static_cast<uint32_t>(mapping.bit);
            if ((remaining_bits & bit) == 0) {
                continue;
            }
            channel_mask = channel_mask | mapping.mask;
            remaining_bits &= ~bit;
        }

        if (remaining_bits != 0) {
            return AudioChannelMask::NONE;
        }

        return channel_mask;
    }

    AudioChannelMask to_channel_mask_from_descriptions(const AudioChannelDescription *p_descriptions,
                                                       const UInt32 p_description_count) {
        AudioChannelMask channel_mask = AudioChannelMask::NONE;
        for (UInt32 description_index = 0; description_index < p_description_count; description_index++) {
            const AudioChannelMask channel_bit = to_channel_mask_from_label(p_descriptions[description_index].mChannelLabel);
            if (channel_bit == AudioChannelMask::NONE) {
                return AudioChannelMask::NONE;
            }
            channel_mask = channel_mask | channel_bit;
        }

        return channel_mask;
    }

    AudioChannelLayoutTag to_channel_layout_tag(const AudioDeviceProperties &p_properties) {
        const AudioChannelMask channel_map = p_properties.channel_map;
        if (channel_map == AudioChannelMask::MONO) {
            return kAudioChannelLayoutTag_Mono;
        }
        if (channel_map == kStereoMask) {
            return kAudioChannelLayoutTag_Stereo;
        }
        if (channel_map == kQuadSideMask) {
            return kAudioChannelLayoutTag_Quadraphonic;
        }
        if (channel_map == kQuadRearMask) {
            return kAudioChannelLayoutTag_WAVE_4_0_B;
        }
        if (channel_map == kThreeZeroMask) {
            return kAudioChannelLayoutTag_MPEG_3_0_A;
        }
        if (channel_map == kFourZeroCenterBackMask) {
            return kAudioChannelLayoutTag_MPEG_4_0_A;
        }
        if (channel_map == kFiveZeroSideMask) {
            return kAudioChannelLayoutTag_MPEG_5_0_A;
        }
        if (channel_map == kFiveZeroRearMask) {
            return kAudioChannelLayoutTag_WAVE_5_0_B;
        }
        if (channel_map == kFiveOneSideMask) {
            return kAudioChannelLayoutTag_MPEG_5_1_A;
        }
        if (channel_map == kFiveOneRearMask) {
            return kAudioChannelLayoutTag_WAVE_5_1_B;
        }
        if (channel_map == kSixOneMask) {
            return kAudioChannelLayoutTag_MPEG_6_1_A;
        }
        if (channel_map == kSevenOneFrontMask) {
            return kAudioChannelLayoutTag_MPEG_7_1_A;
        }
        if (channel_map == kSevenOneSurroundMask) {
            return kAudioChannelLayoutTag_MPEG_7_1_C;
        }
        return kAudioChannelLayoutTag_Unknown;
    }
} // namespace

Lowl::Audio::AudioChannelMask Lowl::Audio::CoreAudioLayout::to_channel_mask(const AudioChannelLayout &p_layout) {
    switch (p_layout.mChannelLayoutTag) {
        case kAudioChannelLayoutTag_UseChannelBitmap:
            return to_channel_mask_from_bitmap(p_layout.mChannelBitmap);
        case kAudioChannelLayoutTag_UseChannelDescriptions:
            return to_channel_mask_from_descriptions(p_layout.mChannelDescriptions, p_layout.mNumberChannelDescriptions);
        case kAudioChannelLayoutTag_Mono:
            return kMonoMask;
        case kAudioChannelLayoutTag_Stereo:
        case kAudioChannelLayoutTag_StereoHeadphones:
            return kStereoMask;
        case kAudioChannelLayoutTag_MPEG_3_0_A:
        case kAudioChannelLayoutTag_MPEG_3_0_B:
        case kAudioChannelLayoutTag_AC3_3_0:
            return kThreeZeroMask;
        case kAudioChannelLayoutTag_Quadraphonic:
        case kAudioChannelLayoutTag_ITU_2_2:
            return kQuadSideMask;
        case kAudioChannelLayoutTag_WAVE_4_0_B:
            return kQuadRearMask;
        case kAudioChannelLayoutTag_MPEG_4_0_A:
        case kAudioChannelLayoutTag_MPEG_4_0_B:
            return kFourZeroCenterBackMask;
        case kAudioChannelLayoutTag_MPEG_5_0_A:
        case kAudioChannelLayoutTag_MPEG_5_0_B:
        case kAudioChannelLayoutTag_MPEG_5_0_C:
        case kAudioChannelLayoutTag_MPEG_5_0_D:
            return kFiveZeroSideMask;
        case kAudioChannelLayoutTag_WAVE_5_0_B:
            return kFiveZeroRearMask;
        case kAudioChannelLayoutTag_MPEG_5_1_A:
        case kAudioChannelLayoutTag_MPEG_5_1_B:
        case kAudioChannelLayoutTag_MPEG_5_1_C:
        case kAudioChannelLayoutTag_MPEG_5_1_D:
            return kFiveOneSideMask;
        case kAudioChannelLayoutTag_WAVE_5_1_B:
            return kFiveOneRearMask;
        case kAudioChannelLayoutTag_MPEG_6_1_A:
            return kSixOneMask;
        case kAudioChannelLayoutTag_MPEG_7_1_A:
        case kAudioChannelLayoutTag_MPEG_7_1_B:
            return kSevenOneFrontMask;
        case kAudioChannelLayoutTag_MPEG_7_1_C:
        case kAudioChannelLayoutTag_WAVE_7_1:
            return kSevenOneSurroundMask;
        default:
            return AudioChannelMask::NONE;
    }
}

std::vector<uint8_t> Lowl::Audio::CoreAudioLayout::create_channel_layout_data(
    const AudioDeviceProperties &p_properties) {
    const AudioChannelLayoutTag layout_tag = to_channel_layout_tag(p_properties);
    if (layout_tag == kAudioChannelLayoutTag_Unknown) {
        return {};
    }
    if (AudioChannelLayoutTag_GetNumberOfChannels(layout_tag) != get_channel_num(p_properties.channel)) {
        return {};
    }

    std::vector<uint8_t> layout_data(sizeof(AudioChannelLayout), 0);
    auto *layout = reinterpret_cast<AudioChannelLayout *>(layout_data.data());
    layout->mChannelLayoutTag = layout_tag;
    layout->mChannelBitmap = 0;
    layout->mNumberChannelDescriptions = 0;
    return layout_data;
}

#endif /* LOWL_DRIVER_CORE_AUDIO */
