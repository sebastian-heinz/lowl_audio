#include "lowl_audio_channel_converter.h"

Lowl::Audio::AudioFrame Lowl::Audio::AudioChannelConverter::to_stereo(AudioFrame p_audio_frame) const {
    p_audio_frame[0] = p_audio_frame[0];
    p_audio_frame[1] = p_audio_frame[0];
    return p_audio_frame;
}

Lowl::Audio::AudioFrame Lowl::Audio::AudioChannelConverter::to_mono(AudioFrame p_audio_frame) const {
    p_audio_frame[0] = (p_audio_frame[0] + p_audio_frame[1]) * 0.5;
    p_audio_frame[1] = p_audio_frame[0];
    return p_audio_frame;
}

std::vector<Lowl::Audio::AudioFrame>
Lowl::Audio::AudioChannelConverter::convert(std::vector<AudioFrame> p_audio_frames, const ConvertFn p_convert_fn) const {
    std::vector<AudioFrame> converted_audio_frames = std::vector<AudioFrame>();
    for (AudioFrame audio_frame : p_audio_frames) {
        AudioFrame converted_frame = (this->*p_convert_fn)(audio_frame);
        converted_audio_frames.push_back(converted_frame);
    }
    return converted_audio_frames;
}

std::vector<Lowl::Audio::AudioFrame>
Lowl::Audio::AudioChannelConverter::convert(Lowl::Audio::AudioChannel p_from, Lowl::Audio::AudioChannel p_to, std::vector<AudioFrame> audio_data,
                                     Error error) const {
    if (p_from == p_to) {
        error.set_error(ErrorCode::Error);
        return audio_data;
    }
    if (p_from == AudioChannel::None || p_to == AudioChannel::None) {
        error.set_error(ErrorCode::Error);
        return audio_data;
    }
    if (p_from == AudioChannel::Mono && p_to == AudioChannel::Stereo) {
        return convert(audio_data, &AudioChannelConverter::to_stereo);
    }
    if (p_from == AudioChannel::Stereo && p_to == AudioChannel::Mono) {
        return convert(audio_data, &AudioChannelConverter::to_mono);
    }
    error.set_error(ErrorCode::Error);
    return audio_data;
}

std::unique_ptr<Lowl::Audio::AudioData>
Lowl::Audio::AudioChannelConverter::convert(AudioChannel p_to, std::shared_ptr<AudioData> p_audio_data, Error error) const {
    std::vector<AudioFrame> frames = convert(p_audio_data->get_channel(), p_to, p_audio_data->get_frames(), error);
    if (error.has_error()) {
        return nullptr;
    }
    std::unique_ptr<AudioData> audio_data = std::make_unique<AudioData>(
            frames,
            p_audio_data->get_sample_rate(),
            p_to
    );
    audio_data->set_name(p_audio_data->get_name());
    return audio_data;
}