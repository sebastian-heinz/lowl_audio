#ifndef LOWL_AUDIO_CORE_AUDIO_LAYOUT_H
#define LOWL_AUDIO_CORE_AUDIO_LAYOUT_H

#ifdef LOWL_DRIVER_CORE_AUDIO

#include <AudioUnit/AudioUnit.h>

#include <vector>

#include "audio/lowl_audio_channel.h"

namespace Lowl::Audio::CoreAudioLayout {
    ChannelLayout to_channel_layout(const AudioChannelLayout &p_layout);

    std::vector<uint8_t> create_channel_layout_data(const ChannelLayout &p_layout);
} // namespace Lowl::Audio::CoreAudioLayout

#endif /* LOWL_DRIVER_CORE_AUDIO */
#endif /* LOWL_AUDIO_CORE_AUDIO_LAYOUT_H */
