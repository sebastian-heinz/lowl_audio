#ifndef LOWL_AUDIO_CHANNEL_CONVERTER_H
#define LOWL_AUDIO_CHANNEL_CONVERTER_H

#include "lowl_error.h"

#include "audio/source/lowl_audio_data.h"
#include "audio/lowl_audio_channel.h"

#include <memory>

namespace Lowl::Audio {
    class ChannelConverter {
    public:
        std::unique_ptr<Lowl::Audio::AudioData> convert(AudioChannel p_to,
                                                        std::shared_ptr<Lowl::Audio::AudioData> p_audio_data,
                                                        Error &error) const;

        ~ChannelConverter() = default;
    };
}
#endif
