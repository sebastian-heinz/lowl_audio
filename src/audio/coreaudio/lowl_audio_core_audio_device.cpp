#ifdef LOWL_DRIVER_CORE_AUDIO

#include "lowl_audio_core_audio_device.h"

#include "lowl_logger.h"
#include "audio/lowl_audio_setting.h"

#include "audio/coreaudio/lowl_audio_core_audio_utilities.h"

static OSStatus osx_audio_callback(
        void *inRefCon,
        AudioUnitRenderActionFlags *ioActionFlags,
        const AudioTimeStamp *inTimeStamp,
        UInt32 inBusNumber,
        UInt32 inNumberFrames,
        AudioBufferList *_Nullable ioData
) {
    Lowl::Audio::CoreAudioDevice *device = (Lowl::Audio::CoreAudioDevice *) inRefCon;
    return device->audio_callback(ioActionFlags, inTimeStamp, inBusNumber, inNumberFrames, ioData);
}

static void osx_start_stop_callback(
        void *inRefCon,
        AudioUnit inUnit,
        AudioUnitPropertyID inID,
        AudioUnitScope inScope,
        AudioUnitElement inElement
) {
    Lowl::Audio::CoreAudioDevice *device = (Lowl::Audio::CoreAudioDevice *) inRefCon;
    device->start_stop_callback(inUnit, inID, inScope, inElement);
}

OSStatus osx_property_callback(
        AudioObjectID inObjectID,
        UInt32 inNumberAddresses,
        const AudioObjectPropertyAddress *inAddresses,
        void *_Nullable inClientData) {
    Lowl::Audio::CoreAudioDevice *device = (Lowl::Audio::CoreAudioDevice *) inClientData;
    return device->property_callback(inObjectID, inNumberAddresses, inAddresses);
}

OSStatus Lowl::Audio::CoreAudioDevice::audio_callback(
        AudioUnitRenderActionFlags *ioActionFlags,
        const AudioTimeStamp *inTimeStamp,
        UInt32 inBusNumber,
        UInt32 inNumberFrames,
        AudioBufferList *ioData
) {
    unsigned long bytes_per_frame =
            get_sample_size_bytes(audio_device_properties.sample_format) *
            ioData->mBuffers[0].mNumberChannels; // TODO asset mNumberChannels == audio_device_properties-channels
    unsigned long frames_per_buffer = static_cast<unsigned long>(ioData->mBuffers[0].mDataByteSize / bytes_per_frame);
    void *dst = ioData->mBuffers[0].mData;
    write_frames(dst, frames_per_buffer, bytes_per_frame);
    return noErr;
}

void Lowl::Audio::CoreAudioDevice::start_stop_callback(AudioUnit inUnit, AudioUnitPropertyID inID,
                                                       AudioUnitScope inScope, AudioUnitElement inElement) {

}

OSStatus Lowl::Audio::CoreAudioDevice::property_callback(AudioObjectID inObjectID, UInt32 inNumberAddresses,
                                                         const AudioObjectPropertyAddress *inAddresses) {
    AudioDevicePropertyID inPropertyID = inAddresses->mSelector;
    switch (inPropertyID) {
        case kAudioDeviceProcessorOverload:
            break;
    }
    return noErr;
}

Lowl::Audio::CoreAudioDevice::CoreAudioDevice(_constructor_tag ct) : AudioDevice(ct) {
    audio_device_properties = AudioDeviceProperties();
    device_id = 0;
    audio_unit = nullptr;
    hog_pid = -1;
}

std::unique_ptr<Lowl::Audio::CoreAudioDevice> Lowl::Audio::CoreAudioDevice::construct(
        const std::string &p_driver_name,
        AudioObjectID p_device_id,
        Error &error
) {
    LOWL_LOG_DEBUG_F("construct->%u - enter", p_device_id);

    std::string device_name = Lowl::Audio::CoreAudioUtilities::get_device_name(p_device_id, error);
    if (error.has_error()) {
        LOWL_LOG_DEBUG_F("construct->%u - get_device_name::FAILED", p_device_id);
        return nullptr;
    }
    LOWL_LOG_DEBUG_F("construct->%u (%s) - get_device_name::OK", p_device_id, device_name.c_str());

    uint32_t output_stream_count = Lowl::Audio::CoreAudioUtilities::get_num_stream(
            p_device_id,
            kAudioDevicePropertyScopeOutput,
            error
    );
    if (error.has_error()) {
        return nullptr;
    }
    LOWL_LOG_DEBUG_F("Device:%u - output_stream_count: %d", p_device_id, output_stream_count);
    if (output_stream_count <= 0) {
        error.set_error(ErrorCode::NoAudioOutput);
        return nullptr;
    }

    std::vector<AudioObjectID> output_streams = Lowl::Audio::CoreAudioUtilities::get_stream_ids(
            p_device_id,
            kAudioDevicePropertyScopeOutput,
            error
    );
    if (error.has_error()) {
        return nullptr;
    }
    LOWL_LOG_DEBUG_F("Device:%u - output_streams count: %zu", p_device_id, output_streams.size());

    std::vector<AudioDeviceProperties> audio_device_properties = create_device_properties(p_device_id);

    std::unique_ptr<CoreAudioDevice> device = std::make_unique<CoreAudioDevice>(_constructor_tag{});
    device->name = "[" + p_driver_name + "] " + device_name;
    device->device_id = p_device_id;
    device->properties = audio_device_properties;
    LOWL_LOG_DEBUG_F("Device:%u - created", p_device_id);

    return device;
}

void Lowl::Audio::CoreAudioDevice::start(AudioDeviceProperties p_audio_device_properties,
                                         std::shared_ptr<AudioSource> p_audio_source,
                                         Error &error) {

    if (!p_audio_device_properties.is_supported) {
        error.set_error(Lowl::ErrorCode::Error);
        return;
    }
    audio_device_properties = p_audio_device_properties;
    audio_source = p_audio_source;


    audio_unit = create_audio_unit(device_id, error);
    if (error.has_error()) {
        LOWL_LOG_ERROR_F("failed on create_audio_unit (device:%u)", device_id);
        return;
    }

    CoreAudioUtilities::add_property_listener(
            device_id,
            kAudioDeviceProcessorOverload,
            kAudioDevicePropertyScopeOutput,
            osx_property_callback,
            this,
            error
    );
    if (error.has_error()) {
        LOWL_LOG_ERROR_F("failed to add property listener (device:%u)", device_id);
        return;
    }

    OSStatus result = AudioUnitAddPropertyListener(
            audio_unit,
            kAudioOutputUnitProperty_IsRunning,
            &osx_start_stop_callback,
            this
    );
    if (result != noErr) {
        LOWL_LOG_ERROR_F("failed to add isRunning listener (device:%u, OSStatus:%u)", device_id, result);
        error.set_vendor_error(result, Error::VendorError::CoreAudioVendorError);
        return;
    }

    SampleCount frames_per_buffer = CoreAudioUtilities::set_frames_per_buffer(device_id, 64, error);
    if (error.has_error()) {
        LOWL_LOG_ERROR_F("failed to set_frames_per_buffer (device:%u)", device_id);
        return;
    }

    CoreAudioUtilities::set_render_quality(
            audio_unit,
            kAudioUnitScope_Global,
            CoreAudioUtilities::kOutputBus,
            kRenderQuality_High,
            error
    );
    if (error.has_error()) {
        LOWL_LOG_ERROR_F("failed to set_render_quality (device:%u)", device_id);
        return;
    }

    AudioStreamBasicDescription description = create_description(audio_device_properties);
    result = AudioUnitSetProperty(
            audio_unit,
            kAudioUnitProperty_StreamFormat,
            kAudioUnitScope_Input,
            CoreAudioUtilities::kOutputBus,
            &description,
            sizeof(AudioStreamBasicDescription)
    );
    if (result != noErr) {
        LOWL_LOG_ERROR_F("failed to set AudioStreamBasicDescription (device:%u, OSStatus:%u)", device_id, result);
        error.set_vendor_error(result, Error::VendorError::CoreAudioVendorError);
        return;
    }

    // if (p_audio_device_properties.exclusive_mode) {
    //     pid_t output_hog_pid = CoreAudioUtilities::get_output_hog_pid(device_id, error);
    //     if (error.has_error()) {
    //         LOWL_LOG_ERROR_F("Device:%u - failed to check hog status", device_id);
    //     } else if (output_hog_pid == -1) {
    //         hog_pid = getpid();
    //         CoreAudioUtilities::set_output_hog_device_pid(device_id, hog_pid, error);
    //         if (error.has_error()) {
    //             LOWL_LOG_ERROR_F("Device:%u - failed to hog", device_id);
    //             hog_pid = CoreAudioUtilities::freeHogDevice;
    //         } else {
    //             LOWL_LOG_DEBUG_F("Device:%u - hogged (hog_pid:%u)", device_id, hog_pid);
    //         }
    //     } else {
    //         LOWL_LOG_ERROR_F("Device:%u - failed to hog (output_hog_pid:%u, getpid():%u)",
    //                          device_id, output_hog_pid, getpid()
    //         );
    //     }
    // }

    CoreAudioUtilities::set_maximum_frames_per_slice(
            audio_unit,
            kAudioUnitScope_Input,
            CoreAudioUtilities::kOutputBus,
            frames_per_buffer,
            error
    );
    if (error.has_error()) {
        LOWL_LOG_ERROR_F("failed to set_maximum_frames_per_slice (device:%u)", device_id);
        return;
    }

    SampleCount max_frames_per_buffer = CoreAudioUtilities::get_maximum_frames_per_slice(
            audio_unit,
            kAudioUnitScope_Global,
            CoreAudioUtilities::kOutputBus,
            error
    );
    if (error.has_error()) {
        LOWL_LOG_ERROR_F("failed to get_maximum_frames_per_slice (device:%u)", device_id);
        return;
    }

    // todo compare set with get, to verify

    AURenderCallbackStruct render_callback;
    render_callback.inputProc = &osx_audio_callback;
    render_callback.inputProcRefCon = this;
    result = AudioUnitSetProperty(
            audio_unit,
            kAudioUnitProperty_SetRenderCallback,
            kAudioUnitScope_Output,
            CoreAudioUtilities::kOutputBus,
            &render_callback,
            sizeof(render_callback)
    );
    if (result != noErr) {
        LOWL_LOG_ERROR_F("failed to set render callback (device:%u, OSStatus:%u)", device_id, result);
        error.set_vendor_error(result, Error::VendorError::CoreAudioVendorError);
        return;
    }

    result = AudioUnitInitialize(audio_unit);
    if (result != noErr) {
        LOWL_LOG_ERROR_F("failed at AudioUnitInitialize (device:%u, OSStatus:%u)", device_id, result);
        error.set_vendor_error(result, Error::VendorError::CoreAudioVendorError);
        return;
    }

    result = AudioOutputUnitStart(audio_unit);
    if (result != noErr) {
        LOWL_LOG_ERROR_F("failed at AudioOutputUnitStart (device:%u, OSStatus:%u)", device_id, result);
        error.set_vendor_error(result, Error::VendorError::CoreAudioVendorError);
        return;
    }
}

void Lowl::Audio::CoreAudioDevice::stop(Lowl::Error &error) {
    OSStatus result = noErr;
    result = AudioOutputUnitStop(audio_unit);
    //result = BlockWhileAudioUnitIsRunning(audio_unit, 0);
    result = AudioUnitReset(audio_unit, kAudioUnitScope_Global, 0);
}

Lowl::Audio::CoreAudioDevice::~CoreAudioDevice() {
    release_hog();
    if (audio_unit) {
        OSStatus result = AudioComponentInstanceDispose(audio_unit);
        if (result != noErr) {
            // log?
        }
        audio_unit = nullptr;
    }
}

std::vector<Lowl::Audio::AudioDeviceProperties>
Lowl::Audio::CoreAudioDevice::create_device_properties(AudioObjectID p_device_id) {

    std::vector<Lowl::Audio::AudioDeviceProperties> properties_list = std::vector<Lowl::Audio::AudioDeviceProperties>();

    Error error;
    AudioUnit _Nullable test_audio_unit = create_audio_unit(p_device_id, error);
    if (error.has_error()) {
        LOWL_LOG_ERROR_F("failed to create_audio_unit (device:%u)", p_device_id);
        return properties_list;
    }

    Lowl::SampleRate default_sample_rate = Lowl::Audio::CoreAudioUtilities::get_device_default_sample_rate(
            p_device_id,
            error
    );
    if (error.has_error()) {
        return properties_list;
    }
    LOWL_LOG_DEBUG_F("Device:%u - default_sample_rate: %f", p_device_id, default_sample_rate);

    uint32_t output_channel_count = Lowl::Audio::CoreAudioUtilities::get_num_channel(
            p_device_id,
            kAudioDevicePropertyScopeOutput,
            error
    );
    if (error.has_error()) {
        return properties_list;
    }
    LOWL_LOG_DEBUG_F("Device:%u - output_channel_count: %d", p_device_id, output_channel_count);

    // device default properties
    AudioStreamBasicDescription descriptionA = CoreAudioUtilities::get_audio_stream_description(
            test_audio_unit,
            kAudioUnitScope_Output,
            error);
    AudioStreamBasicDescription descriptionB = CoreAudioUtilities::get_audio_stream_description(
            test_audio_unit,
            kAudioUnitScope_Input,
            error);


    AudioDeviceProperties default_properties = AudioDeviceProperties();
    default_properties.sample_rate = default_sample_rate;
    default_properties.channel = get_channel(output_channel_count);
    default_properties.sample_format = SampleFormat::FLOAT_32;
    default_properties.channel_map = AudioChannelMask::LEFT | AudioChannelMask::RIGHT;

    if (test_device_properties(p_device_id, test_audio_unit, default_properties)) {
        default_properties.is_supported = true;
        properties_list.push_back(default_properties);
    } else {
        LOWL_LOG_ERROR_F("Device:%u - default properties failed test (%s)",
                         p_device_id,
                         default_properties.to_string().c_str()
        );
    }

    // test other capabilities
    std::vector<double> test_sample_rates = Lowl::Audio::AudioSetting::get_test_sample_rates();
    std::vector<SampleFormat> test_sample_formats = Lowl::Audio::AudioSetting::get_test_sample_formats();
    for (unsigned long sample_format_index = 0;
         sample_format_index < test_sample_formats.size(); sample_format_index++) {
        for (unsigned long sample_rate_index = 0; sample_rate_index < test_sample_rates.size(); sample_rate_index++) {

            AudioDeviceProperties test_properties = AudioDeviceProperties();
            test_properties.sample_format = test_sample_formats[sample_format_index];
            test_properties.sample_rate = test_sample_rates[sample_rate_index];
            test_properties.channel = AudioChannel::Stereo;
            test_properties.channel_map = AudioChannelMask::LEFT | AudioChannelMask::RIGHT;
            test_properties.is_supported = true;

            if (!test_device_properties(p_device_id, test_audio_unit, test_properties)) {
                continue;
            }

            test_properties.is_supported = true;
            properties_list.push_back(test_properties);
        }
    }

    std::sort(properties_list.begin(), properties_list.end());
    properties_list.erase(std::unique(properties_list.begin(), properties_list.end()), properties_list.end());

    return properties_list;
}

bool Lowl::Audio::CoreAudioDevice::test_device_properties(AudioObjectID p_device_id, AudioUnit p_audio_unit,
                                                          AudioDeviceProperties p_properties) {

    AudioStreamBasicDescription description = create_description(p_properties);
    OSStatus result = AudioUnitSetProperty(
            p_audio_unit,
            kAudioUnitProperty_StreamFormat,
            kAudioUnitScope_Input,
            CoreAudioUtilities::kOutputBus,
            &description,
            sizeof(AudioStreamBasicDescription)
    );
    if (result != noErr) {
        LOWL_LOG_ERROR_F("failed to set AudioStreamBasicDescription (device:%u, OSStatus:%u)",
                         p_device_id, result
        );
        return false;
    }

    Error error;
    CoreAudioUtilities::set_input_sample_rate(p_audio_unit, p_properties.sample_rate, error);
    if (error.has_error()) {
        LOWL_LOG_ERROR_F("failed to set input sample rate (device:%u)", p_device_id);
        return false;
    }

    SampleRate output_sample_rate = CoreAudioUtilities::get_output_sample_rate(p_audio_unit, error);
    if (error.has_error()) {
        LOWL_LOG_ERROR_F("failed to set input sample rate (device:%u)", p_device_id);
        return false;
    }

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wfloat-equal"
    if (output_sample_rate != p_properties.sample_rate) {
        return false;
    }
#pragma clang diagnostic pop

    return true;
}

AudioUnit _Nullable Lowl::Audio::CoreAudioDevice::create_audio_unit(AudioObjectID p_device_id, Error &error) {
    AudioComponentDescription desc;
    desc.componentType = kAudioUnitType_Output;
    desc.componentSubType = kAudioUnitSubType_HALOutput;
    desc.componentManufacturer = kAudioUnitManufacturer_Apple;
    desc.componentFlags = 0;
    desc.componentFlagsMask = 0;
    AudioComponent comp = AudioComponentFindNext(nullptr, &desc);
    if (!comp) {
        error.set_error(ErrorCode::CoreAudioNoSuitableComponentFound);
        return nullptr;
    }

    AudioUnit _Nullable new_audio_unit;
    OSStatus result = noErr;
    result = AudioComponentInstanceNew(comp, &new_audio_unit);
    if (result != noErr) {
        LOWL_LOG_ERROR_F("failed to create AudioUnit (device:%u, OSStatus:%u)", p_device_id, result);
        error.set_vendor_error(result, Error::VendorError::CoreAudioVendorError);
        if (new_audio_unit) {
            AudioComponentInstanceDispose(new_audio_unit);
            new_audio_unit = nullptr;
        }
        return nullptr;
    }

    result = AudioUnitSetProperty(new_audio_unit,
                                  kAudioOutputUnitProperty_CurrentDevice,
                                  kAudioUnitScope_Global,
                                  CoreAudioUtilities::kOutputBus,
                                  &p_device_id,
                                  sizeof(AudioDeviceID)
    );
    if (result != noErr) {
        LOWL_LOG_ERROR_F("failed to set property: CurrentDevice (device:%u, OSStatus:%u)", p_device_id, result);
        error.set_vendor_error(result, Error::VendorError::CoreAudioVendorError);
        if (new_audio_unit) {
            AudioComponentInstanceDispose(new_audio_unit);
            new_audio_unit = nullptr;
        }
        return nullptr;
    }

    return new_audio_unit;
}

AudioStreamBasicDescription
Lowl::Audio::CoreAudioDevice::create_description(Lowl::Audio::AudioDeviceProperties p_device_properties) {

    unsigned long channel_num = Lowl::Audio::get_channel_num(p_device_properties.channel);

    AudioStreamBasicDescription description;
    description.mFormatID = kAudioFormatLinearPCM;
    description.mSampleRate = p_device_properties.sample_rate;
    description.mFramesPerPacket = 1;
    description.mBitsPerChannel = static_cast<UInt32>(get_sample_size_bits(p_device_properties.sample_format));
    description.mBytesPerPacket = static_cast<UInt32>(get_sample_size_bytes(p_device_properties.sample_format) *
                                                      channel_num);
    description.mBytesPerFrame = static_cast<UInt32>(get_sample_size_bytes(p_device_properties.sample_format) *
                                                     channel_num);
    description.mChannelsPerFrame = static_cast<UInt32>(channel_num);

    switch (p_device_properties.sample_format) {
        case Lowl::Audio::SampleFormat::FLOAT_32: {
            description.mFormatFlags = kAudioFormatFlagsNativeFloatPacked;
            break;
        }
        case Lowl::Audio::SampleFormat::INT_32: {
            description.mFormatFlags = kAudioFormatFlagIsSignedInteger
                                       | kAudioFormatFlagsNativeEndian
                                       | kAudioFormatFlagIsPacked;
            break;
        }
        case Lowl::Audio::SampleFormat::INT_24: {
            description.mFormatFlags = kAudioFormatFlagIsSignedInteger
                                       | kAudioFormatFlagsNativeEndian
                                       | kAudioFormatFlagIsPacked;
            break;
        }
        case Lowl::Audio::SampleFormat::INT_16: {
            description.mFormatFlags = kAudioFormatFlagIsSignedInteger
                                       | kAudioFormatFlagsNativeEndian
                                       | kAudioFormatFlagIsPacked;
            break;
        }
        case Lowl::Audio::SampleFormat::INT_8: {
            description.mFormatFlags = kAudioFormatFlagIsSignedInteger
                                       | kAudioFormatFlagsNativeEndian
                                       | kAudioFormatFlagIsPacked;
            break;
        }
        case Lowl::Audio::SampleFormat::U_INT_8: {
            return {};
        }
        case Lowl::Audio::SampleFormat::FLOAT_64: {
            description.mFormatFlags = kAudioFormatFlagsNativeFloatPacked;
            break;
        }
        case Lowl::Audio::SampleFormat::Unknown: {
            return {};
        }
    }


    return description;
}

void Lowl::Audio::CoreAudioDevice::release_hog() {
    Error error;

    if (hog_pid == getpid()) {
        //i hogged
        CoreAudioUtilities::set_output_hog_device_pid(device_id, CoreAudioUtilities::freeHogDevice, error);
        LOWL_LOG_DEBUG_F("Device:%u - un-hogged (hog_pid:%u)", device_id, hog_pid);
    }

    pid_t output_hog_pid = CoreAudioUtilities::get_output_hog_pid(device_id, error);
    if (hog_pid == output_hog_pid) {
        // i hogged but different process
        CoreAudioUtilities::set_output_hog_device_pid(device_id, CoreAudioUtilities::freeHogDevice, error);
        LOWL_LOG_DEBUG_F("Device:%u - un-hogged (hog_pid:%u,getpid():%u)", device_id, hog_pid, getpid());
    }

}


#endif /* LOWL_DRIVER_CORE_AUDIO */