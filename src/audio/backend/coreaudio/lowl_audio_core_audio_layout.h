#ifndef LOWL_AUDIO_CORE_AUDIO_LAYOUT_H
#define LOWL_AUDIO_CORE_AUDIO_LAYOUT_H

#ifdef LOWL_DRIVER_CORE_AUDIO

#include <AudioUnit/AudioUnit.h>

#include <vector>

#include "audio/backend/lowl_audio_device_properties.h"

namespace Lowl::Audio::CoreAudioLayout {
    AudioChannelMask to_channel_mask(const AudioChannelLayout &p_layout);

    std::vector<uint8_t> create_channel_layout_data(const AudioDeviceProperties &p_properties);
} // namespace Lowl::Audio::CoreAudioLayout

#endif /* LOWL_DRIVER_CORE_AUDIO */
#endif /* LOWL_AUDIO_CORE_AUDIO_LAYOUT_H */
