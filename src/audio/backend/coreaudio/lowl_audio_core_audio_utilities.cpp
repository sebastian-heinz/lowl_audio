#ifdef LOWL_DRIVER_CORE_AUDIO

#include "lowl_audio_core_audio_utilities.h"
#include "lowl_audio_core_audio_layout.h"

#include <algorithm>

std::string Lowl::Audio::CoreAudioUtilities::get_device_name(AudioObjectID p_device_id, Lowl::Error &error) {
    CFStringRef name_cf_ref;
    uint32_t name_cf_ref_size = sizeof(name_cf_ref);
    AudioObjectPropertyAddress name_property = {kAudioObjectPropertyName,
                                                kAudioObjectPropertyScopeGlobal, // kAudioDevicePropertyScopeOutput,
                                                kAudioObjectPropertyElementMain};
    OSStatus result =
        AudioObjectGetPropertyData(p_device_id, &name_property, 0, nullptr, &name_cf_ref_size, &name_cf_ref);
    if (result != noErr) {
        error.set_vendor_error(result, Error::VendorError::CoreAudioVendorError);
        return std::string();
    }

    long device_name_str_size =
        CFStringGetMaximumSizeForEncoding(CFStringGetLength(name_cf_ref), kCFStringEncodingUTF8);
    char *device_name_str = new char[(unsigned long)device_name_str_size + 1];
    CFStringGetCString(name_cf_ref, device_name_str, device_name_str_size + 1, kCFStringEncodingUTF8);
    CFRelease(name_cf_ref);
    std::string device_name = std::string(device_name_str);
    delete[] device_name_str;

    return device_name;
}

uint32_t Lowl::Audio::CoreAudioUtilities::get_num_stream(AudioObjectID p_device_id,
                                                         AudioObjectPropertyScope p_scope,
                                                         Lowl::Error &error) {
    if (p_scope != kAudioDevicePropertyScopeInput && p_scope != kAudioDevicePropertyScopeOutput) {
        error.set_error(ErrorCode::InvalidParameter);
        return 0;
    }

    AudioObjectPropertyAddress stream_property = {
        kAudioDevicePropertyStreams, p_scope, kAudioObjectPropertyElementMain};
    uint32_t stream_data_size = 0;
    OSStatus result = AudioObjectGetPropertyDataSize(p_device_id, &stream_property, 0, nullptr, &stream_data_size);
    if (result != noErr) {
        error.set_vendor_error(result, Error::VendorError::CoreAudioVendorError);
        return 0;
    }
    uint32_t stream_count = stream_data_size / sizeof(AudioStreamID);
    return stream_count;
}

Lowl::SampleRate Lowl::Audio::CoreAudioUtilities::get_device_default_sample_rate(AudioObjectID p_device_id,
                                                                                 Lowl::Error &error) {
    uint32_t default_sample_rate_size = sizeof(Float64);
    Float64 default_sample_rate;
    AudioObjectPropertyAddress default_sample_rate_property = {
        kAudioDevicePropertyNominalSampleRate, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMain};
    OSStatus result = AudioObjectGetPropertyData(
        p_device_id, &default_sample_rate_property, 0, nullptr, &default_sample_rate_size, &default_sample_rate);
    if (result != noErr) {
        error.set_vendor_error(result, Error::VendorError::CoreAudioVendorError);
        return 0.0;
    }
    return static_cast<Lowl::SampleRate>(default_sample_rate);
}

uint32_t Lowl::Audio::CoreAudioUtilities::get_num_channel(AudioObjectID p_device_id,
                                                          AudioObjectPropertyScope p_scope,
                                                          Lowl::Error &error) {
    if (p_scope != kAudioDevicePropertyScopeInput && p_scope != kAudioDevicePropertyScopeOutput) {
        error.set_error(ErrorCode::InvalidParameter);
        return 0;
    }

    AudioObjectPropertyAddress stream_config_property = {
        kAudioDevicePropertyStreamConfiguration, p_scope, kAudioObjectPropertyElementMain};
    uint32_t stream_config_data_size = 0;
    OSStatus result =
        AudioObjectGetPropertyDataSize(p_device_id, &stream_config_property, 0, nullptr, &stream_config_data_size);
    if (result != noErr) {
        error.set_vendor_error(result, Error::VendorError::CoreAudioVendorError);
        return 0;
    }

    std::vector<uint8_t> stream_config_buffer(stream_config_data_size);
    AudioBufferList *audio_buffers = reinterpret_cast<AudioBufferList *>(stream_config_buffer.data());

    result = AudioObjectGetPropertyData(
        p_device_id, &stream_config_property, 0, nullptr, &stream_config_data_size, audio_buffers);
    if (result != noErr) {
        error.set_vendor_error(result, Error::VendorError::CoreAudioVendorError);
        return 0;
    }

    UInt32 num_channel = 0;
    for (int i = 0; i < audio_buffers->mNumberBuffers; ++i) {
        num_channel += audio_buffers->mBuffers[i].mNumberChannels;
    }

    return num_channel;
}

Lowl::Audio::ChannelLayout Lowl::Audio::CoreAudioUtilities::get_channel_layout(AudioObjectID p_device_id,
                                                                               AudioObjectPropertyScope p_scope,
                                                                               Lowl::Error &error) {
    AudioObjectPropertyAddress channel_layout_property = {
        kAudioDevicePropertyPreferredChannelLayout, p_scope, kAudioObjectPropertyElementMain};
    uint32_t channel_layout_size = 0;
    OSStatus result =
        AudioObjectGetPropertyDataSize(p_device_id, &channel_layout_property, 0, nullptr, &channel_layout_size);
    if (result != noErr) {
        error.set_vendor_error(result, Error::VendorError::CoreAudioVendorError);
        return {};
    }
    if (channel_layout_size < sizeof(AudioChannelLayout)) {
        return {};
    }

    std::vector<uint8_t> channel_layout_buffer(channel_layout_size);
    AudioChannelLayout *channel_layout = reinterpret_cast<AudioChannelLayout *>(channel_layout_buffer.data());
    result =
        AudioObjectGetPropertyData(p_device_id, &channel_layout_property, 0, nullptr, &channel_layout_size, channel_layout);
    if (result != noErr) {
        error.set_vendor_error(result, Error::VendorError::CoreAudioVendorError);
        return {};
    }

    return CoreAudioLayout::to_channel_layout(*channel_layout);
}

void Lowl::Audio::CoreAudioUtilities::set_audio_unit_channel_layout(AudioUnit p_audio_unit,
                                                                    AudioUnitScope p_scope,
                                                                    AudioUnitElement p_element,
                                                                    const void *p_channel_layout_data,
                                                                    UInt32 p_channel_layout_size,
                                                                    Lowl::Error &error) {
    const OSStatus result = AudioUnitSetProperty(p_audio_unit,
                                                 kAudioUnitProperty_AudioChannelLayout,
                                                 p_scope,
                                                 p_element,
                                                 p_channel_layout_data,
                                                 p_channel_layout_size);
    if (result != noErr) {
        error.set_vendor_error(result, Error::VendorError::CoreAudioVendorError);
        return;
    }
}

std::vector<AudioObjectID> Lowl::Audio::CoreAudioUtilities::get_device_ids(Lowl::Error &error) {
    OSStatus result = kAudioHardwareNoError;

    AudioObjectPropertyAddress device_property = {
        kAudioHardwarePropertyDevices, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMain};
    uint32_t device_property_size;
    result =
        AudioObjectGetPropertyDataSize(kAudioObjectSystemObject, &device_property, 0, nullptr, &device_property_size);
    if (result != noErr) {
        error.set_vendor_error(result, Error::VendorError::CoreAudioVendorError);
        return std::vector<AudioObjectID>();
    }

    uint32_t audio_object_size = sizeof(AudioObjectID);
    uint32_t device_count = device_property_size / audio_object_size;
    std::vector<AudioObjectID> device_ids = std::vector<AudioObjectID>(device_count);
    result = AudioObjectGetPropertyData(
        kAudioObjectSystemObject, &device_property, 0, nullptr, &device_property_size, device_ids.data());
    if (result != noErr) {
        error.set_vendor_error(result, Error::VendorError::CoreAudioVendorError);
        return std::vector<AudioObjectID>();
    }

    return device_ids;
}

AudioObjectID Lowl::Audio::CoreAudioUtilities::get_default_device_id(Lowl::Error &error) {
    AudioObjectPropertyAddress default_device_property = {
        kAudioHardwarePropertyDefaultOutputDevice, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMain};
    AudioObjectID default_out_device_id = kAudioObjectUnknown;

    uint32_t audio_object_size = sizeof(AudioObjectID);
    OSStatus result = AudioObjectGetPropertyData(
        kAudioObjectSystemObject, &default_device_property, 0, nullptr, &audio_object_size, &default_out_device_id);
    if (result != noErr) {
        error.set_vendor_error(result, Error::VendorError::CoreAudioVendorError);
        return 0;
    }
    return default_out_device_id;
}

Lowl::SampleCount Lowl::Audio::CoreAudioUtilities::get_device_latency(AudioObjectID p_device_id,
                                                                      AudioObjectPropertyScope p_scope,
                                                                      Lowl::Error &error) {
    UInt32 device_latency;
    UInt32 device_property_size = sizeof(UInt32);    AudioObjectPropertyAddress latency_property = {
        kAudioDevicePropertyLatency, p_scope, kAudioObjectPropertyElementMain};
    OSStatus result =
        AudioObjectGetPropertyData(p_device_id, &latency_property, 0, nullptr, &device_property_size, &device_latency);
    if (result != noErr) {
        error.set_vendor_error(result, Error::VendorError::CoreAudioVendorError);
        return 0;
    }
    return device_latency;
}

Lowl::SampleCount Lowl::Audio::CoreAudioUtilities::get_safety_offset(AudioObjectID p_device_id,
                                                                     AudioObjectPropertyScope p_scope,
                                                                     Lowl::Error &error) {
    UInt32 safety_offset;
    UInt32 safety_offset_property_size = sizeof(UInt32);    AudioObjectPropertyAddress safety_offset_property = {
        kAudioDevicePropertySafetyOffset, p_scope, kAudioObjectPropertyElementMain};
    OSStatus result = AudioObjectGetPropertyData(
        p_device_id, &safety_offset_property, 0, nullptr, &safety_offset_property_size, &safety_offset);
    if (result != noErr) {
        error.set_vendor_error(result, Error::VendorError::CoreAudioVendorError);
        return 0;
    }
    return safety_offset;
}

Lowl::SampleCount Lowl::Audio::CoreAudioUtilities::get_stream_latency(AudioStreamID p_stream_id,
                                                                      AudioObjectPropertyScope p_scope,
                                                                      Lowl::Error &error) {
    UInt32 stream_latency;
    UInt32 property_size = sizeof(UInt32);    AudioObjectPropertyAddress property = {kAudioStreamPropertyLatency, p_scope, kAudioObjectPropertyElementMain};
    OSStatus result = AudioObjectGetPropertyData(p_stream_id, &property, 0, nullptr, &property_size, &stream_latency);
    if (result != noErr) {
        error.set_vendor_error(result, Error::VendorError::CoreAudioVendorError);
        return 0;
    }
    return stream_latency;
}

Lowl::SampleCount Lowl::Audio::CoreAudioUtilities::get_latency_high(AudioObjectID p_device_id,
                                                                    AudioStreamID p_stream_id,
                                                                    AudioObjectPropertyScope p_scope,
                                                                    Lowl::Error &error) {
    SampleCount device_latency = get_device_latency(p_device_id, p_scope, error);
    if (error.has_error()) {
        return 0;
    }
    SampleCount stream_latency = get_stream_latency(p_stream_id, p_scope, error);
    if (error.has_error()) {
        return 0;
    }
    SampleCount safety_offset = get_safety_offset(p_device_id, p_scope, error);
    if (error.has_error()) {
        return 0;
    }
    SampleCount buffer_frame_size = get_buffer_frame_size(p_device_id, p_scope, error);
    if (error.has_error()) {
        return 0;
    }
    return device_latency + stream_latency + safety_offset + buffer_frame_size;
}

Lowl::SampleCount Lowl::Audio::CoreAudioUtilities::get_latency_low(UInt32 desired_size,
                                                                   AudioObjectID p_device_id,
                                                                   AudioStreamID p_stream_id,
                                                                   AudioObjectPropertyScope p_scope,
                                                                   Lowl::Error &error) {
    SampleCount device_latency = get_device_latency(p_device_id, p_scope, error);
    if (error.has_error()) {
        return 0;
    }
    SampleCount stream_latency = get_stream_latency(p_stream_id, p_scope, error);
    if (error.has_error()) {
        return 0;
    }
    SampleCount safety_offset = get_safety_offset(p_device_id, p_scope, error);
    if (error.has_error()) {
        return 0;
    }
    AudioValueRange audio_range = get_buffer_frame_size_range(p_device_id, p_scope, error);
    if (error.has_error()) {
        return 0;
    }

    desired_size = std::max(desired_size, (UInt32)audio_range.mMinimum);
    desired_size = std::min(desired_size, (UInt32)audio_range.mMaximum);

    return device_latency + stream_latency + safety_offset + desired_size;
}

Lowl::SampleCount Lowl::Audio::CoreAudioUtilities::get_buffer_frame_size(AudioObjectID p_device_id,
                                                                         AudioObjectPropertyScope p_scope,
                                                                         Lowl::Error &error) {
    UInt32 buffer_frame_size;
    UInt32 property_size = sizeof(UInt32);    AudioObjectPropertyAddress latency_property = {
        kAudioDevicePropertyBufferFrameSize, p_scope, kAudioObjectPropertyElementMain};
    OSStatus result =
        AudioObjectGetPropertyData(p_device_id, &latency_property, 0, nullptr, &property_size, &buffer_frame_size);
    if (result != noErr) {
        error.set_vendor_error(result, Error::VendorError::CoreAudioVendorError);
        return 0;
    }
    return buffer_frame_size;
}

std::vector<AudioObjectID> Lowl::Audio::CoreAudioUtilities::get_stream_ids(AudioObjectID p_device_id,
                                                                           AudioObjectPropertyScope p_scope,
                                                                           Lowl::Error &error) {
    uint32_t stream_count = get_num_stream(p_device_id, p_scope, error);
    if (error.has_error()) {
        return {};
    }
    if (stream_count == 0) {
        error.set_error(ErrorCode::InvalidParameter);
        return {};
    }
    uint32_t property_size = stream_count * sizeof(AudioStreamID);
    AudioObjectPropertyAddress property = {kAudioDevicePropertyStreams, p_scope, kAudioObjectPropertyElementMain};
    std::vector<AudioObjectID> streams = std::vector<AudioObjectID>(stream_count);
    OSStatus result = AudioObjectGetPropertyData(p_device_id, &property, 0, nullptr, &property_size, streams.data());
    if (result != noErr) {
        error.set_vendor_error(result, Error::VendorError::CoreAudioVendorError);
        return std::vector<AudioObjectID>();
    }
    return streams;
}

AudioValueRange Lowl::Audio::CoreAudioUtilities::get_buffer_frame_size_range(AudioObjectID p_device_id,
                                                                             AudioObjectPropertyScope p_scope,
                                                                             Lowl::Error &error) {
    AudioValueRange audio_range;
    UInt32 property_size = sizeof(audio_range);
    AudioObjectPropertyAddress property = {
        kAudioDevicePropertyBufferFrameSizeRange, p_scope, kAudioObjectPropertyElementMain};
    OSStatus result = AudioObjectGetPropertyData(p_device_id, &property, 0, nullptr, &property_size, &audio_range);
    if (result != noErr) {
        error.set_vendor_error(result, Error::VendorError::CoreAudioVendorError);
        return AudioValueRange{};
    }
    return audio_range;
}

void Lowl::Audio::CoreAudioUtilities::set_buffer_frame_size(AudioObjectID p_device_id,
                                                            AudioObjectPropertyScope p_scope,
                                                            UInt32 p_frames_per_buffer,
                                                            Lowl::Error &error) {
    UInt32 property_size = sizeof(UInt32);
    AudioObjectPropertyAddress property = {
        kAudioDevicePropertyBufferFrameSize, p_scope, kAudioObjectPropertyElementMain};
    OSStatus result =
        AudioObjectSetPropertyData(p_device_id, &property, 0, nullptr, property_size, &p_frames_per_buffer);
    if (result != noErr) {
        error.set_vendor_error(result, Error::VendorError::CoreAudioVendorError);
        return;
    }
}

void Lowl::Audio::CoreAudioUtilities::set_maximum_frames_per_slice(AudioUnit p_audio_unit,
                                                                   AudioUnitScope p_scope,
                                                                   AudioUnitElement p_element,
                                                                   UInt32 p_maximum_frames_per_slice,
                                                                   Lowl::Error &error) {
    OSStatus result = AudioUnitSetProperty(p_audio_unit,
                                           kAudioUnitProperty_MaximumFramesPerSlice,
                                           p_scope,
                                           p_element,
                                           &p_maximum_frames_per_slice,
                                           sizeof(p_maximum_frames_per_slice));
    if (result != noErr) {
        error.set_vendor_error(result, Error::VendorError::CoreAudioVendorError);
        return;
    }
}

Lowl::SampleCount Lowl::Audio::CoreAudioUtilities::get_maximum_frames_per_slice(AudioUnit p_audio_unit,
                                                                                AudioUnitScope p_scope,
                                                                                AudioUnitElement p_element,
                                                                                Lowl::Error &error) {
    SampleCount max_frames_per_buffer = 0;
    UInt32 max_frames_per_buffer_size = sizeof(max_frames_per_buffer);
    OSStatus result = AudioUnitGetProperty(p_audio_unit,
                                           kAudioUnitProperty_MaximumFramesPerSlice,
                                           p_scope,
                                           p_element,
                                           &max_frames_per_buffer,
                                           &max_frames_per_buffer_size);
    if (result != noErr) {
        error.set_vendor_error(result, Error::VendorError::CoreAudioVendorError);
        return 0;
    }
    return max_frames_per_buffer;
}

void Lowl::Audio::CoreAudioUtilities::add_property_listener(AudioObjectID p_device_id,
                                                            AudioObjectPropertySelector p_property,
                                                            AudioObjectPropertyScope p_scope,
                                                            AudioObjectPropertyListenerProc p_proc,
                                                            void *p_user_data,
                                                            Lowl::Error &error) {
    AudioObjectPropertyAddress property = {p_property, p_scope, kAudioObjectPropertyElementMain};
    OSStatus result = AudioObjectAddPropertyListener(p_device_id, &property, p_proc, p_user_data);
    if (result == kAudioHardwareIllegalOperationError) {
        // Already registered.
        return;
    }
    if (result != noErr) {
        error.set_vendor_error(result, Error::VendorError::CoreAudioVendorError);
        return;
    }
}

void Lowl::Audio::CoreAudioUtilities::remove_property_listener(AudioObjectID p_device_id,
                                                               AudioObjectPropertySelector p_property,
                                                               AudioObjectPropertyScope p_scope,
                                                               AudioObjectPropertyListenerProc p_proc,
                                                               void *p_user_data,
                                                               Lowl::Error &error) {
    AudioObjectPropertyAddress property = {p_property, p_scope, kAudioObjectPropertyElementMain};
    OSStatus result = AudioObjectRemovePropertyListener(p_device_id, &property, p_proc, p_user_data);
    if (result == kAudioHardwareIllegalOperationError) {
        return;
    }
    if (result != noErr) {
        error.set_vendor_error(result, Error::VendorError::CoreAudioVendorError);
        return;
    }
}

void Lowl::Audio::CoreAudioUtilities::set_render_quality(AudioUnit p_audio_unit,
                                                         AudioUnitScope p_scope,
                                                         AudioUnitElement p_element,
                                                         UInt32 p_render_quality,
                                                         Lowl::Error &error) {
    OSStatus result = AudioUnitSetProperty(p_audio_unit,
                                           kAudioUnitProperty_RenderQuality,
                                           p_scope,
                                           p_element,
                                           &p_render_quality,
                                           sizeof(p_render_quality));
    if (result != noErr) {
        error.set_vendor_error(result, Error::VendorError::CoreAudioVendorError);
        return;
    }
}

pid_t Lowl::Audio::CoreAudioUtilities::get_output_hog_pid(AudioObjectID p_device_id, Lowl::Error &error) {
    pid_t hog_pid = -1;
    UInt32 property_size = sizeof(hog_pid);

    AudioObjectPropertyAddress hog_property = {
        kAudioDevicePropertyHogMode, kAudioDevicePropertyScopeOutput, kAudioObjectPropertyElementMain};

    OSStatus result = AudioObjectGetPropertyData(p_device_id, &hog_property, 0, nullptr, &property_size, &hog_pid);
    if (result != noErr) {
        error.set_vendor_error(result, Error::VendorError::CoreAudioVendorError);
        return -1;
    }

    // -1 = No process has hogged
    // getpid() = our process hogged
    // else -> another process hogged
    return hog_pid;
}

void Lowl::Audio::CoreAudioUtilities::set_output_hog_device_pid(AudioObjectID p_device_id,
                                                                pid_t p_hog_pid,
                                                                Lowl::Error &error) {
    UInt32 property_size = sizeof(UInt32);
    AudioObjectPropertyAddress hog_property = {
        kAudioDevicePropertyHogMode, kAudioDevicePropertyScopeOutput, kAudioObjectPropertyElementMain};
    OSStatus result = AudioObjectSetPropertyData(p_device_id, &hog_property, 0, nullptr, property_size, &p_hog_pid);
    if (result != noErr) {
        error.set_vendor_error(result, Error::VendorError::CoreAudioVendorError);
        return;
    }
}

void Lowl::Audio::CoreAudioUtilities::set_input_sample_rate(AudioUnit p_audio_unit,
                                                            Lowl::SampleRate p_sample_rate,
                                                            Lowl::Error &error) {
    Float64 sample_rate = p_sample_rate;
    OSStatus result = AudioUnitSetProperty(
        p_audio_unit, kAudioUnitProperty_SampleRate, kAudioUnitScope_Input, 0, &sample_rate, sizeof(sample_rate));
    if (result != noErr) {
        error.set_vendor_error(result, Error::VendorError::CoreAudioVendorError);
        return;
    }
}

Lowl::SampleRate Lowl::Audio::CoreAudioUtilities::get_output_sample_rate(AudioUnit p_audio_unit, Lowl::Error &error) {
    Float64 sample_rate = 0.0;
    UInt32 sample_rate_size = sizeof(sample_rate);
    OSStatus result = AudioUnitGetProperty(
        p_audio_unit, kAudioUnitProperty_SampleRate, kAudioUnitScope_Output, 0, &sample_rate, &sample_rate_size);
    if (result != noErr) {
        error.set_vendor_error(result, Error::VendorError::CoreAudioVendorError);
        return 0;
    }
    return sample_rate;
}

Lowl::SampleCount Lowl::Audio::CoreAudioUtilities::set_frames_per_buffer(AudioObjectID p_device_id,
                                                                         SampleCount p_frames_per_buffer,
                                                                         Lowl::Error &error) {
    SampleCount requested_frames_per_buffer = p_frames_per_buffer;
    SampleCount actual_frames_per_buffer = 0;
    CoreAudioUtilities::set_buffer_frame_size(
        p_device_id, kAudioDevicePropertyScopeOutput, requested_frames_per_buffer, error);
    if (error.has_error()) {
        return 0;
    }
    actual_frames_per_buffer =
        CoreAudioUtilities::get_buffer_frame_size(p_device_id, kAudioDevicePropertyScopeOutput, error);
    if (error.has_error()) {
        return 0;
    }

    // Did we get the size we asked for?
    if (actual_frames_per_buffer != requested_frames_per_buffer) {
        AudioValueRange range =
            CoreAudioUtilities::get_buffer_frame_size_range(p_device_id, kAudioDevicePropertyScopeOutput, error);
        if (error.has_error()) {
            return 0;
        }
        if (requested_frames_per_buffer < range.mMinimum) {
            requested_frames_per_buffer = static_cast<UInt32>(range.mMinimum);
        } else if (requested_frames_per_buffer > range.mMaximum) {
            requested_frames_per_buffer = static_cast<UInt32>(range.mMaximum);
        }
        CoreAudioUtilities::set_buffer_frame_size(
            p_device_id, kAudioDevicePropertyScopeOutput, requested_frames_per_buffer, error);
        if (error.has_error()) {
            return 0;
        }
        actual_frames_per_buffer =
            CoreAudioUtilities::get_buffer_frame_size(p_device_id, kAudioDevicePropertyScopeOutput, error);
        if (error.has_error()) {
            return 0;
        }
    }
    return actual_frames_per_buffer;
}

AudioStreamBasicDescription Lowl::Audio::CoreAudioUtilities::get_audio_stream_description(AudioUnit p_audio_unit,
                                                                                          AudioUnitScope p_scope,
                                                                                          Lowl::Error &error) {
    AudioStreamBasicDescription description = AudioStreamBasicDescription();
    UInt32 description_size = sizeof(description);
    OSStatus result = AudioUnitGetProperty(p_audio_unit,
                                           kAudioUnitProperty_StreamFormat,
                                           p_scope,
                                           CoreAudioUtilities::kOutputBus,
                                           &description,
                                           &description_size);
    if (result != noErr) {
        error.set_vendor_error(result, Error::VendorError::CoreAudioVendorError);
        return AudioStreamBasicDescription();
    }
    return description;
}

#endif /* LOWL_DRIVER_CORE_AUDIO */
