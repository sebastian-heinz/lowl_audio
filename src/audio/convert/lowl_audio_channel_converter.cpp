#include "lowl_audio_channel_converter.h"

std::unique_ptr<Lowl::Audio::AudioData>
Lowl::Audio::ChannelConverter::convert(AudioChannel p_to, std::shared_ptr<AudioData> p_audio_data, Error &error) const {
    if (!p_audio_data) {
        error.set_error(ErrorCode::ConvertAudioChannelInvalid);
        return nullptr;
    }

    const AudioChannel p_from = p_audio_data->get_channel();
    if (p_from == p_to) {
        const size_t frame_count = p_audio_data->get_frame_count();
        const size_t channel_count = p_audio_data->get_channel_num();
        std::unique_ptr<Sample[]> storage;
        if (frame_count > 0 && channel_count > 0) {
            storage = std::make_unique<Sample[]>(frame_count * channel_count);
            for (size_t channel_index = 0; channel_index < channel_count; channel_index++) {
                const Sample *src = p_audio_data->get_channel_data(static_cast<uint8_t>(channel_index));
                if (src != nullptr) {
                    std::copy_n(src, frame_count, storage.get() + channel_index * frame_count);
                }
            }
        }
        std::unique_ptr<AudioData> audio_data =
            std::make_unique<AudioData>(std::move(storage), frame_count, p_audio_data->get_sample_rate(), p_to);
        audio_data->set_name(p_audio_data->get_name());
        return audio_data;
    }
    if (p_from == AudioChannel::None || p_to == AudioChannel::None) {
        error.set_error(ErrorCode::ConvertAudioChannelInvalid);
        return nullptr;
    }

    const size_t frame_count = p_audio_data->get_frame_count();
    const size_t channel_count = get_channel_num(p_to);
    std::unique_ptr<Sample[]> storage;
    if (frame_count > 0 && channel_count > 0) {
        storage = std::make_unique<Sample[]>(frame_count * channel_count);
    }

    if (p_from == AudioChannel::Mono && p_to == AudioChannel::Stereo) {
        const Sample *mono = p_audio_data->get_channel_data(0);
        Sample *left = storage.get();
        Sample *right = storage.get() + frame_count;
        for (size_t frame_index = 0; frame_index < frame_count; frame_index++) {
            const Sample sample = mono ? mono[frame_index] : static_cast<Sample>(0);
            left[frame_index] = sample;
            right[frame_index] = sample;
        }
    } else if (p_from == AudioChannel::Stereo && p_to == AudioChannel::Mono) {
        const Sample *left = p_audio_data->get_channel_data(0);
        const Sample *right = p_audio_data->get_channel_data(1);
        Sample *mono = storage.get();
        for (size_t frame_index = 0; frame_index < frame_count; frame_index++) {
            const Sample left_sample = left ? left[frame_index] : static_cast<Sample>(0);
            const Sample right_sample = right ? right[frame_index] : static_cast<Sample>(0);
            mono[frame_index] = static_cast<Sample>((left_sample + right_sample) * 0.5);
        }
    } else {
        error.set_error(ErrorCode::ConvertAudioChannelNotSupported);
        return nullptr;
    }

    std::unique_ptr<AudioData> audio_data =
        std::make_unique<AudioData>(std::move(storage), frame_count, p_audio_data->get_sample_rate(), p_to);
    audio_data->set_name(p_audio_data->get_name());
    return audio_data;
}
