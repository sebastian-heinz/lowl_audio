#ifdef LOWL_DRIVER_CORE_AUDIO

#include "lowl_audio_core_audio_layout.h"

#include <cstddef>

namespace {
    using Lowl::Audio::ChannelLayout;
    using Lowl::Audio::Speaker;

    bool to_label_from_speaker(const Speaker p_speaker, AudioChannelLabel &r_label) {
        switch (p_speaker) {
            case Speaker::FrontLeft:
                r_label = kAudioChannelLabel_Left;
                return true;
            case Speaker::FrontRight:
                r_label = kAudioChannelLabel_Right;
                return true;
            case Speaker::FrontCenter:
                r_label = kAudioChannelLabel_Center;
                return true;
            case Speaker::LowFrequency:
                r_label = kAudioChannelLabel_LFEScreen;
                return true;
            case Speaker::BackLeft:
                r_label = kAudioChannelLabel_RearSurroundLeft;
                return true;
            case Speaker::BackRight:
                r_label = kAudioChannelLabel_RearSurroundRight;
                return true;
            case Speaker::FrontLeftOfCenter:
                r_label = kAudioChannelLabel_LeftCenter;
                return true;
            case Speaker::FrontRightOfCenter:
                r_label = kAudioChannelLabel_RightCenter;
                return true;
            case Speaker::BackCenter:
                r_label = kAudioChannelLabel_CenterSurround;
                return true;
            case Speaker::SideLeft:
                r_label = kAudioChannelLabel_LeftSideSurround;
                return true;
            case Speaker::SideRight:
                r_label = kAudioChannelLabel_RightSideSurround;
                return true;
            case Speaker::TopCenter:
                r_label = kAudioChannelLabel_TopCenterSurround;
                return true;
            case Speaker::TopFrontLeft:
                r_label = kAudioChannelLabel_VerticalHeightLeft;
                return true;
            case Speaker::TopFrontCenter:
                r_label = kAudioChannelLabel_VerticalHeightCenter;
                return true;
            case Speaker::TopFrontRight:
                r_label = kAudioChannelLabel_VerticalHeightRight;
                return true;
            case Speaker::TopBackLeft:
                r_label = kAudioChannelLabel_TopBackLeft;
                return true;
            case Speaker::TopBackCenter:
                r_label = kAudioChannelLabel_TopBackCenter;
                return true;
            case Speaker::TopBackRight:
                r_label = kAudioChannelLabel_TopBackRight;
                return true;
            default:
                return false;
        }
    }

    Speaker to_speaker_from_label(const AudioChannelLabel p_label) {
        switch (p_label) {
            case kAudioChannelLabel_Mono:
            case kAudioChannelLabel_Center:
                return Speaker::FrontCenter;
            case kAudioChannelLabel_Left:
                return Speaker::FrontLeft;
            case kAudioChannelLabel_Right:
                return Speaker::FrontRight;
            case kAudioChannelLabel_LFEScreen:
            case kAudioChannelLabel_LFE2:
            case kAudioChannelLabel_LFE3:
                return Speaker::LowFrequency;
            case kAudioChannelLabel_LeftSurround:
            case kAudioChannelLabel_LeftSurroundDirect:
            case kAudioChannelLabel_LeftSideSurround:
                return Speaker::SideLeft;
            case kAudioChannelLabel_RightSurround:
            case kAudioChannelLabel_RightSurroundDirect:
            case kAudioChannelLabel_RightSideSurround:
                return Speaker::SideRight;
            case kAudioChannelLabel_LeftCenter:
                return Speaker::FrontLeftOfCenter;
            case kAudioChannelLabel_RightCenter:
                return Speaker::FrontRightOfCenter;
            case kAudioChannelLabel_CenterSurround:
            case kAudioChannelLabel_CenterSurroundDirect:
                return Speaker::BackCenter;
            case kAudioChannelLabel_RearSurroundLeft:
            case kAudioChannelLabel_LeftBackSurround:
                return Speaker::BackLeft;
            case kAudioChannelLabel_RearSurroundRight:
            case kAudioChannelLabel_RightBackSurround:
                return Speaker::BackRight;
            case kAudioChannelLabel_TopCenterSurround:
                return Speaker::TopCenter;
            case kAudioChannelLabel_VerticalHeightLeft:
                return Speaker::TopFrontLeft;
            case kAudioChannelLabel_VerticalHeightCenter:
                return Speaker::TopFrontCenter;
            case kAudioChannelLabel_VerticalHeightRight:
                return Speaker::TopFrontRight;
            case kAudioChannelLabel_TopBackLeft:
                return Speaker::TopBackLeft;
            case kAudioChannelLabel_TopBackCenter:
                return Speaker::TopBackCenter;
            case kAudioChannelLabel_TopBackRight:
                return Speaker::TopBackRight;
            default:
                return static_cast<Speaker>(0);
        }
    }

    ChannelLayout to_channel_layout_from_bitmap(const AudioChannelBitmap p_bitmap) {
        struct BitmapMapping {
            AudioChannelBitmap bit;
            Speaker speaker;
        };

        static constexpr BitmapMapping bit_mappings[] = {
            {kAudioChannelBit_Left, Speaker::FrontLeft},
            {kAudioChannelBit_Right, Speaker::FrontRight},
            {kAudioChannelBit_Center, Speaker::FrontCenter},
            {kAudioChannelBit_LFEScreen, Speaker::LowFrequency},
            {kAudioChannelBit_LeftSurround, Speaker::SideLeft},
            {kAudioChannelBit_RightSurround, Speaker::SideRight},
            {kAudioChannelBit_LeftCenter, Speaker::FrontLeftOfCenter},
            {kAudioChannelBit_RightCenter, Speaker::FrontRightOfCenter},
            {kAudioChannelBit_CenterSurround, Speaker::BackCenter},
            {kAudioChannelBit_LeftSurroundDirect, Speaker::SideLeft},
            {kAudioChannelBit_RightSurroundDirect, Speaker::SideRight},
            {kAudioChannelBit_TopCenterSurround, Speaker::TopCenter},
            {kAudioChannelBit_VerticalHeightLeft, Speaker::TopFrontLeft},
            {kAudioChannelBit_VerticalHeightCenter, Speaker::TopFrontCenter},
            {kAudioChannelBit_VerticalHeightRight, Speaker::TopFrontRight},
            {kAudioChannelBit_TopBackLeft, Speaker::TopBackLeft},
            {kAudioChannelBit_TopBackCenter, Speaker::TopBackCenter},
            {kAudioChannelBit_TopBackRight, Speaker::TopBackRight},
        };

        uint32_t remaining_bits = static_cast<uint32_t>(p_bitmap);
        uint32_t speaker_mask = 0;
        for (const BitmapMapping &mapping : bit_mappings) {
            const uint32_t bit = static_cast<uint32_t>(mapping.bit);
            if ((remaining_bits & bit) == 0u) {
                continue;
            }
            speaker_mask |= Lowl::Audio::speaker_bits(mapping.speaker);
            remaining_bits &= ~bit;
        }

        if (remaining_bits != 0u) {
            return {};
        }
        return ChannelLayout::from_mask(speaker_mask);
    }

    ChannelLayout to_channel_layout_from_descriptions(const AudioChannelDescription *p_descriptions,
                                                      const UInt32 p_description_count) {
        uint32_t speaker_mask = 0;
        for (UInt32 description_index = 0; description_index < p_description_count; description_index++) {
            const Speaker speaker = to_speaker_from_label(p_descriptions[description_index].mChannelLabel);
            if (speaker == static_cast<Speaker>(0)) {
                return {};
            }
            speaker_mask |= Lowl::Audio::speaker_bits(speaker);
        }
        return ChannelLayout::from_mask(speaker_mask);
    }
} // namespace

Lowl::Audio::ChannelLayout Lowl::Audio::CoreAudioLayout::to_channel_layout(const AudioChannelLayout &p_layout) {
    switch (p_layout.mChannelLayoutTag) {
        case kAudioChannelLayoutTag_UseChannelBitmap:
            return to_channel_layout_from_bitmap(p_layout.mChannelBitmap);
        case kAudioChannelLayoutTag_UseChannelDescriptions:
            return to_channel_layout_from_descriptions(p_layout.mChannelDescriptions,
                                                       p_layout.mNumberChannelDescriptions);
        case kAudioChannelLayoutTag_Mono:
            return ChannelLayout::Mono;
        case kAudioChannelLayoutTag_Stereo:
        case kAudioChannelLayoutTag_StereoHeadphones:
            return ChannelLayout::Stereo;
        case kAudioChannelLayoutTag_MPEG_3_0_A:
        case kAudioChannelLayoutTag_MPEG_3_0_B:
        case kAudioChannelLayoutTag_AC3_3_0:
            return ChannelLayout::Surround_3_0;
        case kAudioChannelLayoutTag_Quadraphonic:
        case kAudioChannelLayoutTag_ITU_2_2:
            return ChannelLayout::Quad_Side;
        case kAudioChannelLayoutTag_WAVE_4_0_B:
            return ChannelLayout::Quad;
        case kAudioChannelLayoutTag_MPEG_4_0_A:
        case kAudioChannelLayoutTag_MPEG_4_0_B:
            return ChannelLayout::Surround_4_0;
        case kAudioChannelLayoutTag_MPEG_5_0_A:
        case kAudioChannelLayoutTag_MPEG_5_0_B:
        case kAudioChannelLayoutTag_MPEG_5_0_C:
        case kAudioChannelLayoutTag_MPEG_5_0_D:
            return ChannelLayout::Surround_5_0;
        case kAudioChannelLayoutTag_WAVE_5_0_B:
            return ChannelLayout::Surround_5_0_Rear;
        case kAudioChannelLayoutTag_MPEG_5_1_A:
        case kAudioChannelLayoutTag_MPEG_5_1_B:
        case kAudioChannelLayoutTag_MPEG_5_1_C:
        case kAudioChannelLayoutTag_MPEG_5_1_D:
            return ChannelLayout::Surround_5_1;
        case kAudioChannelLayoutTag_WAVE_5_1_B:
            return ChannelLayout::Surround_5_1_Rear;
        case kAudioChannelLayoutTag_MPEG_6_1_A:
            return ChannelLayout::Surround_6_1;
        case kAudioChannelLayoutTag_MPEG_7_1_A:
        case kAudioChannelLayoutTag_MPEG_7_1_B:
            return ChannelLayout::Surround_7_1_Front;
        case kAudioChannelLayoutTag_MPEG_7_1_C:
        case kAudioChannelLayoutTag_WAVE_7_1:
            return ChannelLayout::Surround_7_1;
        default:
            return {};
    }
}

std::vector<uint8_t> Lowl::Audio::CoreAudioLayout::create_channel_layout_data(const ChannelLayout &p_layout) {
    if (!p_layout.is_valid()) {
        return {};
    }

    const size_t layout_size = offsetof(AudioChannelLayout, mChannelDescriptions) +
                               static_cast<size_t>(p_layout.channel_count) * sizeof(AudioChannelDescription);
    std::vector<uint8_t> layout_data(layout_size, 0);
    auto *layout = reinterpret_cast<AudioChannelLayout *>(layout_data.data());
    layout->mChannelLayoutTag = kAudioChannelLayoutTag_UseChannelDescriptions;
    layout->mChannelBitmap = 0;
    layout->mNumberChannelDescriptions = p_layout.channel_count;
    for (uint8_t channel_index = 0; channel_index < p_layout.channel_count; channel_index++) {
        AudioChannelLabel label = kAudioChannelLabel_Unknown;
        if (!to_label_from_speaker(p_layout.speaker_at(channel_index), label)) {
            return {};
        }
        layout->mChannelDescriptions[channel_index].mChannelLabel = label;
    }
    return layout_data;
}

#endif /* LOWL_DRIVER_CORE_AUDIO */
