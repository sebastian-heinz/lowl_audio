#include "lowl_audio_channel_converter.h"

#include <algorithm>
#include <vector>

#include "audio/lowl_audio_utilities.h"

namespace {
    struct ChannelContribution {
        int src_index = -1;
        float gain = 0.0f;
    };

    constexpr float downmix_gain = 0.70710678f;
    constexpr float surround_fold_gain = 0.70710678f;

    bool is_supported_layout(const Lowl::Audio::ChannelLayout &p_layout) {
        for (uint8_t channel_index = 0; channel_index < p_layout.channel_count; channel_index++) {
            switch (p_layout.speaker_at(channel_index)) {
                case Lowl::Audio::Speaker::FrontLeft:
                case Lowl::Audio::Speaker::FrontRight:
                case Lowl::Audio::Speaker::FrontCenter:
                case Lowl::Audio::Speaker::LowFrequency:
                case Lowl::Audio::Speaker::BackLeft:
                case Lowl::Audio::Speaker::BackRight:
                case Lowl::Audio::Speaker::FrontLeftOfCenter:
                case Lowl::Audio::Speaker::FrontRightOfCenter:
                case Lowl::Audio::Speaker::BackCenter:
                case Lowl::Audio::Speaker::SideLeft:
                case Lowl::Audio::Speaker::SideRight:
                    break;
                case Lowl::Audio::Speaker::TopCenter:
                case Lowl::Audio::Speaker::TopFrontLeft:
                case Lowl::Audio::Speaker::TopFrontCenter:
                case Lowl::Audio::Speaker::TopFrontRight:
                case Lowl::Audio::Speaker::TopBackLeft:
                case Lowl::Audio::Speaker::TopBackCenter:
                case Lowl::Audio::Speaker::TopBackRight:
                    return false;
                default:
                    return false;
            }
        }
        return true;
    }

    void add_contribution(std::vector<ChannelContribution> &p_target,
                          const Lowl::Audio::ChannelLayout &p_source_layout,
                          const Lowl::Audio::Speaker p_speaker,
                          const float p_gain) {
        const int source_index = p_source_layout.index_of(p_speaker);
        if (source_index >= 0) {
            p_target.push_back({source_index, p_gain});
        }
    }

    std::vector<std::vector<ChannelContribution>> build_mix_plan(const Lowl::Audio::ChannelLayout &p_target_layout,
                                                                 const Lowl::Audio::ChannelLayout &p_source_layout) {
        using Lowl::Audio::ChannelLayout;
        using Lowl::Audio::Speaker;

        std::vector<std::vector<ChannelContribution>> plan(p_target_layout.channel_count);
        const bool target_is_mono = p_target_layout == ChannelLayout::Mono;
        const bool target_is_stereo = p_target_layout == ChannelLayout::Stereo;
        const bool source_is_mono = p_source_layout == ChannelLayout::Mono;

        for (uint8_t channel_index = 0; channel_index < p_target_layout.channel_count; channel_index++) {
            const Speaker target_speaker = p_target_layout.speaker_at(channel_index);
            std::vector<ChannelContribution> &contributions = plan[channel_index];

            add_contribution(contributions, p_source_layout, target_speaker, 1.0f);

            switch (target_speaker) {
                case Speaker::FrontLeft:
                    if (source_is_mono && contributions.empty()) {
                        add_contribution(contributions, p_source_layout, Speaker::FrontCenter, 1.0f);
                    }
                    if (target_is_stereo && !source_is_mono) {
                        add_contribution(contributions, p_source_layout, Speaker::FrontCenter, downmix_gain);
                        add_contribution(contributions, p_source_layout, Speaker::FrontLeftOfCenter, downmix_gain);
                        add_contribution(contributions, p_source_layout, Speaker::SideLeft, downmix_gain);
                        if (p_source_layout.has(Speaker::SideLeft) && p_source_layout.has(Speaker::BackLeft)) {
                            add_contribution(contributions, p_source_layout, Speaker::BackLeft, surround_fold_gain);
                        } else {
                            add_contribution(contributions, p_source_layout, Speaker::BackLeft, downmix_gain);
                        }
                    }
                    break;
                case Speaker::FrontRight:
                    if (source_is_mono && contributions.empty()) {
                        add_contribution(contributions, p_source_layout, Speaker::FrontCenter, 1.0f);
                    }
                    if (target_is_stereo && !source_is_mono) {
                        add_contribution(contributions, p_source_layout, Speaker::FrontCenter, downmix_gain);
                        add_contribution(contributions, p_source_layout, Speaker::FrontRightOfCenter, downmix_gain);
                        add_contribution(contributions, p_source_layout, Speaker::SideRight, downmix_gain);
                        if (p_source_layout.has(Speaker::SideRight) && p_source_layout.has(Speaker::BackRight)) {
                            add_contribution(contributions, p_source_layout, Speaker::BackRight, surround_fold_gain);
                        } else {
                            add_contribution(contributions, p_source_layout, Speaker::BackRight, downmix_gain);
                        }
                    }
                    break;
                case Speaker::FrontCenter:
                    if (target_is_mono && contributions.empty()) {
                        add_contribution(contributions, p_source_layout, Speaker::FrontLeft, 0.5f);
                        add_contribution(contributions, p_source_layout, Speaker::FrontRight, 0.5f);
                    }
                    break;
                case Speaker::BackLeft:
                    if (contributions.empty()) {
                        add_contribution(contributions, p_source_layout, Speaker::SideLeft, 1.0f);
                    }
                    if (p_source_layout.has(Speaker::BackLeft) && p_source_layout.has(Speaker::SideLeft) &&
                        !p_target_layout.has(Speaker::SideLeft)) {
                        add_contribution(contributions, p_source_layout, Speaker::SideLeft, surround_fold_gain);
                    }
                    break;
                case Speaker::BackRight:
                    if (contributions.empty()) {
                        add_contribution(contributions, p_source_layout, Speaker::SideRight, 1.0f);
                    }
                    if (p_source_layout.has(Speaker::BackRight) && p_source_layout.has(Speaker::SideRight) &&
                        !p_target_layout.has(Speaker::SideRight)) {
                        add_contribution(contributions, p_source_layout, Speaker::SideRight, surround_fold_gain);
                    }
                    break;
                case Speaker::SideLeft:
                    if (contributions.empty()) {
                        add_contribution(contributions, p_source_layout, Speaker::BackLeft, 1.0f);
                    }
                    if (p_source_layout.has(Speaker::SideLeft) && p_source_layout.has(Speaker::BackLeft) &&
                        !p_target_layout.has(Speaker::BackLeft)) {
                        add_contribution(contributions, p_source_layout, Speaker::BackLeft, surround_fold_gain);
                    }
                    break;
                case Speaker::SideRight:
                    if (contributions.empty()) {
                        add_contribution(contributions, p_source_layout, Speaker::BackRight, 1.0f);
                    }
                    if (p_source_layout.has(Speaker::SideRight) && p_source_layout.has(Speaker::BackRight) &&
                        !p_target_layout.has(Speaker::BackRight)) {
                        add_contribution(contributions, p_source_layout, Speaker::BackRight, surround_fold_gain);
                    }
                    break;
                case Speaker::LowFrequency:
                case Speaker::FrontLeftOfCenter:
                case Speaker::FrontRightOfCenter:
                case Speaker::BackCenter:
                case Speaker::TopCenter:
                case Speaker::TopFrontLeft:
                case Speaker::TopFrontCenter:
                case Speaker::TopFrontRight:
                case Speaker::TopBackLeft:
                case Speaker::TopBackCenter:
                case Speaker::TopBackRight:
                    break;
                default:
                    break;
            }
        }

        return plan;
    }
} // namespace

std::unique_ptr<Lowl::Audio::AudioData>
Lowl::Audio::ChannelConverter::convert(ChannelLayout p_target_layout,
                                       std::shared_ptr<AudioData> p_audio_data,
                                       Error &error) const {
    if (!p_audio_data) {
        error.set_error(ErrorCode::ConvertAudioChannelInvalid);
        return nullptr;
    }

    const ChannelLayout source_layout = p_audio_data->get_channel_layout();
    if (!source_layout.is_valid() || !p_target_layout.is_valid()) {
        error.set_error(ErrorCode::ConvertAudioChannelInvalid);
        return nullptr;
    }
    if (source_layout == p_target_layout) {
        return Utilities::clone_audio_data(p_audio_data);
    }
    if (!is_supported_layout(source_layout) || !is_supported_layout(p_target_layout)) {
        error.set_error(ErrorCode::ConvertAudioChannelNotSupported);
        return nullptr;
    }

    const size_t frame_count = p_audio_data->get_frame_count();
    const uint8_t channel_count = p_target_layout.channel_count;
    std::unique_ptr<Sample[]> storage;
    if (frame_count > 0 && channel_count > 0) {
        storage = std::make_unique<Sample[]>(frame_count * channel_count);
    }

    const std::vector<std::vector<ChannelContribution>> plan = build_mix_plan(p_target_layout, source_layout);
    for (uint8_t channel_index = 0; channel_index < channel_count; channel_index++) {
        Sample *destination = storage ? storage.get() + static_cast<size_t>(channel_index) * frame_count : nullptr;
        if (destination == nullptr) {
            continue;
        }

        std::fill_n(destination, frame_count, static_cast<Sample>(0));
        for (const ChannelContribution &contribution : plan[channel_index]) {
            if (contribution.src_index < 0) {
                continue;
            }
            const Sample *source = p_audio_data->get_channel_data(static_cast<uint8_t>(contribution.src_index));
            if (source == nullptr) {
                continue;
            }
            for (size_t frame_index = 0; frame_index < frame_count; frame_index++) {
                destination[frame_index] += source[frame_index] * contribution.gain;
            }
        }
    }

    std::unique_ptr<AudioData> audio_data = std::make_unique<AudioData>(
        std::move(storage), frame_count, p_audio_data->get_sample_rate(), p_target_layout);
    audio_data->set_name(p_audio_data->get_name());
    return audio_data;
}
