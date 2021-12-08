#ifndef LOWL_AUDIO_CORE_AUDIO_UTILITIES_H
#define LOWL_AUDIO_CORE_AUDIO_UTILITIES_H

#ifdef LOWL_DRIVER_CORE_AUDIO

#include "lowl_typedef.h"
#include "lowl_error.h"

#include <string>
#include <vector>

#include <CoreAudio/AudioHardware.h>

namespace Lowl::Audio {

    class CoreAudioUtilities {

    public:
        static std::vector<AudioObjectID> get_device_ids(Lowl::Error &error);

        static AudioObjectID get_default_device_id(Lowl::Error &error);

        static std::string get_device_name(AudioObjectID p_device_id, Lowl::Error &error);

        static std::vector<AudioObjectID> get_stream_ids(AudioObjectID p_device_id, AudioObjectPropertyScope p_scope, Lowl::Error &error);

        static uint32_t get_num_stream(AudioObjectID p_device_id, AudioObjectPropertyScope p_scope, Lowl::Error &error);

        static uint32_t get_num_channel(AudioObjectID p_device_id, AudioObjectPropertyScope p_scope, Lowl::Error &error);

        static SampleRate get_device_default_sample_rate(AudioObjectID p_device_id, Lowl::Error &error);

        static SampleCount get_latency_low(UInt32 desired_size, AudioObjectID p_device_id, AudioStreamID p_stream_id, AudioObjectPropertyScope p_scope, Lowl::Error &error);

        static SampleCount get_latency_high(AudioObjectID p_device_id, AudioStreamID p_stream_id, AudioObjectPropertyScope p_scope, Lowl::Error &error);

        static SampleCount get_device_latency(AudioObjectID p_device_id, AudioObjectPropertyScope p_scope, Lowl::Error &error);

        static SampleCount get_safety_offset(AudioObjectID p_device_id, AudioObjectPropertyScope p_scope, Lowl::Error &error);

        static SampleCount get_stream_latency(AudioStreamID p_stream_id, AudioObjectPropertyScope p_scope, Lowl::Error &error);

        static SampleCount get_buffer_frame_size(AudioObjectID p_device_id, AudioObjectPropertyScope p_scope, Lowl::Error &error);

        static AudioValueRange get_buffer_frame_size_range(AudioObjectID p_device_id, AudioObjectPropertyScope p_scope, Lowl::Error &error);

        static void set_buffer_frame_size(AudioObjectID p_device_id, AudioObjectPropertyScope p_scope, UInt32 p_frames_per_buffer, Lowl::Error &error);
    };
}

#endif /* LOWL_DRIVER_CORE_AUDIO */
#endif /* LOWL_AUDIO_CORE_AUDIO_UTILITIES_H */
