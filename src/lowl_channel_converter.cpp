# include "lowl_channel_converter.h"

Lowl::AudioFrame Lowl::ChannelConverter::to_stereo(AudioFrame p_audio_frame) const {
    p_audio_frame[0] = p_audio_frame[0];
    p_audio_frame[1] = p_audio_frame[0];
    return p_audio_frame;
}

Lowl::AudioFrame Lowl::ChannelConverter::to_mono(AudioFrame p_audio_frame) const {
    p_audio_frame[0] = (p_audio_frame[0] + p_audio_frame[1]) * 0.5;
    p_audio_frame[1] = p_audio_frame[0];
    return p_audio_frame;
}

std::vector<Lowl::AudioFrame>
Lowl::ChannelConverter::convert(std::vector<AudioFrame> p_audio_frames, const ConvertFn p_convert_fn) const {
    std::vector<AudioFrame> converted_audio_frames = std::vector<AudioFrame>();
    for (AudioFrame audio_frame : p_audio_frames) {
        AudioFrame converted_frame = (this->*p_convert_fn)(audio_frame);
        converted_audio_frames.push_back(converted_frame);
    }
    return converted_audio_frames;
}

std::vector<Lowl::AudioFrame>
Lowl::ChannelConverter::convert(Lowl::Channel p_from, Lowl::Channel p_to, std::vector<AudioFrame> audio_data,
                                Error error) const {
    if (p_from == p_to) {
        error.set_error(ErrorCode::Error);
        return audio_data;
    }
    if (p_from == Channel::None || p_to == Channel::None) {
        error.set_error(ErrorCode::Error);
        return audio_data;
    }
    if (p_from == Channel::Mono && p_to == Channel::Stereo) {
        return convert(audio_data, &ChannelConverter::to_stereo);
    }
    if (p_from == Channel::Stereo && p_to == Channel::Mono) {
        return convert(audio_data, &ChannelConverter::to_mono);
    }
    error.set_error(ErrorCode::Error);
    return audio_data;
}

std::unique_ptr<Lowl::AudioData>
Lowl::ChannelConverter::convert(Channel p_to, std::shared_ptr<AudioData> p_audio_data, Error error) const {
    std::vector<AudioFrame> frames = convert(p_audio_data->get_channel(), p_to, p_audio_data->get_frames(), error);
    if (error.has_error()) {
        return nullptr;
    }
    std::unique_ptr<AudioData> audio_data = std::make_unique<AudioData>(
            frames,
            p_audio_data->get_sample_rate(),
            p_to
    );
    return audio_data;
}