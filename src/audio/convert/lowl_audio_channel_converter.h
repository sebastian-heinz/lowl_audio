#ifndef LOWL_AUDIO_CHANNEL_CONVERTER_H
#define LOWL_AUDIO_CHANNEL_CONVERTER_H

#include "lowl_error.h"

#include "audio/lowl_audio_frame.h"
#include "audio/lowl_audio_data.h"
#include "audio/lowl_audio_channel.h"

#include <memory>

namespace Lowl::Audio {
    class AudioChannelConverter {

    private:
        using ConvertFn = AudioFrame (AudioChannelConverter::*)(AudioFrame) const;

        std::vector<AudioFrame> convert(std::vector<AudioFrame> audio_data, const ConvertFn convert_fn) const;

    public:
        AudioFrame to_stereo(AudioFrame p_audio_frame) const;

        AudioFrame to_mono(AudioFrame p_audio_frame) const;

        std::vector<AudioFrame> convert(AudioChannel p_from, AudioChannel p_to, std::vector<AudioFrame> audio_data, Error error) const;

        std::unique_ptr<Lowl::Audio::AudioData> convert(AudioChannel p_to, std::shared_ptr<Lowl::Audio::AudioData> p_audio_data, Error error) const;

        ~AudioChannelConverter() = default;
    };
}
#endif
