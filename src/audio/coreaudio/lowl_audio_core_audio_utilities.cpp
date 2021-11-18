#ifdef LOWL_DRIVER_CORE_AUDIO

#include "lowl_audio_core_audio_utilities.h"

#include "lowl_logger.h"

std::string Lowl::Audio::CoreAudioUtilities::get_device_name(AudioObjectID p_device_id) {
    CFStringRef name_cf_ref;
    uint32_t name_cf_ref_size = sizeof(name_cf_ref);
    AudioObjectPropertyAddress name_property = {
            kAudioObjectPropertyName,
            kAudioObjectPropertyScopeGlobal,//kAudioDevicePropertyScopeOutput,
            kAudioObjectPropertyElementMaster
    };
    OSStatus result = AudioObjectGetPropertyData(
            p_device_id,
            &name_property,
            0,
            nullptr,
            &name_cf_ref_size,
            &name_cf_ref
    );
    if (result != kAudioHardwareNoError) {
        // err
    }

    long device_name_str_size = CFStringGetMaximumSizeForEncoding(
            CFStringGetLength(name_cf_ref),
            kCFStringEncodingUTF8
    );
    char *device_name_str = new char[(unsigned long) device_name_str_size + 1];
    CFStringGetCString(name_cf_ref, device_name_str, device_name_str_size + 1, kCFStringEncodingUTF8);
    CFRelease(name_cf_ref);
    std::string device_name = std::string(device_name_str);
    delete[] device_name_str;

    return device_name;
}

uint32_t
Lowl::Audio::CoreAudioUtilities::get_num_stream(AudioObjectID p_device_id, AudioObjectPropertyScope p_scope) {
    if (p_scope != kAudioDevicePropertyScopeInput && p_scope != kAudioDevicePropertyScopeOutput) {
        // error
    }

    AudioObjectPropertyAddress stream_property = {
            kAudioDevicePropertyStreams,
            p_scope,
            kAudioObjectPropertyElementMaster
    };
    uint32_t stream_data_size = 0;
    OSStatus status = AudioObjectGetPropertyDataSize(p_device_id,
                                                     &stream_property,
                                                     0,
                                                     nullptr,
                                                     &stream_data_size);
    if (status != kAudioHardwareNoError) {
        // err
    }
    uint32_t stream_count = stream_data_size / sizeof(AudioStreamID);
    return stream_count;
}

Lowl::SampleRate Lowl::Audio::CoreAudioUtilities::get_device_default_sample_rate(AudioObjectID p_device_id) {
    uint32_t default_sample_rate_size = sizeof(Float64);
    Float64 default_sample_rate;
    AudioObjectPropertyAddress default_sample_rate_property = {
            kAudioDevicePropertyNominalSampleRate,
            kAudioObjectPropertyScopeGlobal,
            kAudioObjectPropertyElementMaster
    };
    OSStatus result = AudioObjectGetPropertyData(
            p_device_id,
            &default_sample_rate_property,
            0,
            nullptr,
            &default_sample_rate_size,
            &default_sample_rate
    );
    if (result != kAudioHardwareNoError) {
        LOWL_LOG_WARN("!exclusive_mode_applied");
        default_sample_rate = 0.0;
        // err
    }
    return static_cast<Lowl::SampleRate>(default_sample_rate);
}

uint32_t Lowl::Audio::CoreAudioUtilities::get_num_channel(AudioObjectID p_device_id, AudioObjectPropertyScope p_scope) {
    if (p_scope != kAudioDevicePropertyScopeInput && p_scope != kAudioDevicePropertyScopeOutput) {
        // error
    }

    AudioObjectPropertyAddress stream_config_property = {
            kAudioDevicePropertyStreamConfiguration,
            p_scope,
            kAudioObjectPropertyElementMaster
    };
    uint32_t stream_config_data_size = 0;
    OSStatus status = AudioObjectGetPropertyDataSize(p_device_id,
                                                     &stream_config_property,
                                                     0,
                                                     nullptr,
                                                     &stream_config_data_size);
    if (status != kAudioHardwareNoError) {
        // err
    }

    AudioBufferList *audio_buffers = new AudioBufferList[stream_config_data_size];

    OSStatus result = AudioObjectGetPropertyData(
            p_device_id,
            &stream_config_property,
            0,
            nullptr,
            &stream_config_data_size,
            audio_buffers
    );
    if (result != kAudioHardwareNoError) {
        // err
    }

    UInt32 num_channel = 0;
    for (int i = 0; i < audio_buffers->mNumberBuffers; ++i) {
        num_channel += audio_buffers->mBuffers[i].mNumberChannels;
    }
    delete[] audio_buffers;

    return num_channel;
}

std::vector<AudioObjectID> Lowl::Audio::CoreAudioUtilities::get_device_ids() {
    OSStatus result = kAudioHardwareNoError;

    AudioObjectPropertyAddress device_property = {
            kAudioHardwarePropertyDevices,
            kAudioObjectPropertyScopeGlobal,
            kAudioObjectPropertyElementMaster
    };
    uint32_t device_property_size;
    result = AudioObjectGetPropertyDataSize(kAudioObjectSystemObject,
                                            &device_property,
                                            0,
                                            nullptr,
                                            &device_property_size
    );
    if (result != kAudioHardwareNoError) {
        // err
    }
    uint32_t audio_object_size = sizeof(AudioObjectID);
    uint32_t device_count = device_property_size / audio_object_size;
    std::vector<AudioObjectID> device_ids = std::vector<AudioObjectID>(device_count);
    result = AudioObjectGetPropertyData(kAudioObjectSystemObject,
                                        &device_property,
                                        0,
                                        nullptr,
                                        &device_property_size,
                                        device_ids.data()
    );
    if (result != kAudioHardwareNoError) {
        // err
    }

    return device_ids;
}

AudioObjectID Lowl::Audio::CoreAudioUtilities::get_default_device_id() {


    AudioObjectPropertyAddress default_device_property = {
            kAudioHardwarePropertyDefaultOutputDevice,
            kAudioObjectPropertyScopeGlobal,
            kAudioObjectPropertyElementMaster
    };
    AudioObjectID default_out_device_id = kAudioObjectUnknown;

    uint32_t audio_object_size = sizeof(AudioObjectID);
    OSStatus result = AudioObjectGetPropertyData(kAudioObjectSystemObject,
                                                 &default_device_property,
                                                 0,
                                                 nullptr,
                                                 &audio_object_size,
                                                 &default_out_device_id
    );
    if (result != kAudioHardwareNoError) {
        // err
    }
    return default_out_device_id;
}

Lowl::SampleCount
Lowl::Audio::CoreAudioUtilities::get_device_latency(AudioObjectID p_device_id, AudioObjectPropertyScope p_scope) {
    UInt32 device_latency;
    UInt32 device_property_size = sizeof(UInt32); // todo get prop size?
    AudioObjectPropertyAddress latency_property = {
            kAudioDevicePropertyLatency,
            p_scope,
            kAudioObjectPropertyElementMaster
    };
    OSStatus result = AudioObjectGetPropertyData(
            p_device_id,
            &latency_property,
            0,
            nullptr,
            &device_property_size,
            &device_latency
    );
    if (result != kAudioHardwareNoError) {
        // err
    }
    return device_latency;
}

Lowl::SampleCount
Lowl::Audio::CoreAudioUtilities::get_safety_offset(AudioObjectID p_device_id, AudioObjectPropertyScope p_scope) {
    UInt32 safety_offset;
    UInt32 safety_offset_property_size = sizeof(UInt32); // todo get prop size?
    AudioObjectPropertyAddress safety_offset_property = {
            kAudioDevicePropertySafetyOffset,
            p_scope,
            kAudioObjectPropertyElementMaster
    };
    OSStatus result = AudioObjectGetPropertyData(
            p_device_id,
            &safety_offset_property,
            0,
            nullptr,
            &safety_offset_property_size,
            &safety_offset
    );
    if (result != kAudioHardwareNoError) {
        // err
    }
    return safety_offset;
}

Lowl::SampleCount
Lowl::Audio::CoreAudioUtilities::get_stream_latency(AudioStreamID p_stream_id, AudioObjectPropertyScope p_scope) {
    UInt32 stream_latency;
    UInt32 property_size = sizeof(UInt32); // todo get prop size?
    AudioObjectPropertyAddress property = {
            kAudioStreamPropertyLatency,
            p_scope,
            kAudioObjectPropertyElementMaster
    };
    OSStatus result = AudioObjectGetPropertyData(
            p_stream_id,
            &property,
            0,
            nullptr,
            &property_size,
            &stream_latency
    );
    if (result != kAudioHardwareNoError) {
        // err
    }
    return stream_latency;
}

Lowl::SampleCount Lowl::Audio::CoreAudioUtilities::get_latency(AudioObjectID p_device_id, AudioStreamID p_stream_id,
                                                               AudioObjectPropertyScope p_scope) {
    SampleCount device_latency = get_device_latency(p_device_id, p_scope);
    SampleCount stream_latency = get_stream_latency(p_stream_id, p_scope);
    SampleCount safety_offset = get_safety_offset(p_device_id, p_scope);
    SampleCount buffer_frame_size = get_buffer_frame_size(p_device_id, p_scope);
    return device_latency + stream_latency + safety_offset + buffer_frame_size;
}

Lowl::SampleCount
Lowl::Audio::CoreAudioUtilities::get_buffer_frame_size(AudioObjectID p_device_id, AudioObjectPropertyScope p_scope) {
    UInt32 buffer_frame_size;
    UInt32 property_size = sizeof(UInt32); // todo get prop size?
    AudioObjectPropertyAddress latency_property = {
            kAudioDevicePropertyBufferFrameSize,
            p_scope,
            kAudioObjectPropertyElementMaster
    };
    OSStatus result = AudioObjectGetPropertyData(
            p_device_id,
            &latency_property,
            0,
            nullptr,
            &property_size,
            &buffer_frame_size
    );
    if (result != kAudioHardwareNoError) {
        // err
    }
    return buffer_frame_size;
}

std::vector<AudioObjectID>
Lowl::Audio::CoreAudioUtilities::get_stream_ids(AudioObjectID p_device_id, AudioObjectPropertyScope p_scope) {
    uint32_t stream_count = get_num_stream(p_device_id, p_scope);
    if (stream_count <= 0) {
        // TODO err
    }
    uint32_t property_size = stream_count * sizeof(AudioStreamID);
    AudioObjectPropertyAddress property = {
            kAudioDevicePropertyStreams,
            p_scope,
            kAudioObjectPropertyElementMaster
    };
    std::vector<AudioObjectID> streams = std::vector<AudioObjectID>(stream_count);
    OSStatus status = AudioObjectGetPropertyData(p_device_id,
                                                 &property,
                                                 0,
                                                 nullptr,
                                                 &property_size,
                                                 streams.data()
    );
    if (status != kAudioHardwareNoError) {
        // err
    }
    return streams;
}


#endif /* LOWL_DRIVER_CORE_AUDIO */

