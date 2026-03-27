#ifndef LOWL_AUDIO_CHANNEL_CONVERTER_H
#define LOWL_AUDIO_CHANNEL_CONVERTER_H

#include <memory>

#include "audio/lowl_audio_channel.h"
#include "audio/source/lowl_audio_data.h"
#include "lowl_error.h"

namespace Lowl::Audio {
    class ChannelConverter {
    public:
        std::unique_ptr<Lowl::Audio::AudioData>
        convert(ChannelLayout p_target_layout, std::shared_ptr<Lowl::Audio::AudioData> p_audio_data, Error &error) const;

        ~ChannelConverter() = default;
    };
} // namespace Lowl::Audio
#endif
