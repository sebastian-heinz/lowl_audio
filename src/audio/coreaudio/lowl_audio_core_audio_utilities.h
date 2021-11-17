#ifndef LOWL_AUDIO_CORE_AUDIO_UTILITIES_H
#define LOWL_AUDIO_CORE_AUDIO_UTILITIES_H

#ifdef LOWL_DRIVER_CORE_AUDIO

#include "lowl_typedef.h"

#include <string>
#include <vector>

#include <CoreAudio/AudioHardware.h>

namespace Lowl::Audio {

    class CoreAudioUtilities {

    public:
        static std::vector<AudioObjectID> get_device_ids();

        static AudioObjectID get_default_device_id();

        static std::string get_device_name(AudioObjectID p_device_id);

        static uint32_t get_num_stream(AudioObjectID p_device_id, AudioObjectPropertyScope p_scope);

        static uint32_t get_num_channel(AudioObjectID p_device_id, AudioObjectPropertyScope p_scope);

        static Lowl::SampleRate get_device_default_sample_rate(AudioObjectID p_device_id);

        static Lowl::TimeSeconds get_latency(AudioObjectID p_device_id, AudioStreamID p_stream_id, AudioObjectPropertyScope p_scope);
    };
}

#endif /* LOWL_DRIVER_CORE_AUDIO */
#endif /* LOWL_AUDIO_CORE_AUDIO_UTILITIES_H */
