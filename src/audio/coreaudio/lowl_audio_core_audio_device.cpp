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
    long bytesPerFrame = sizeof(float) * ioData->mBuffers[0].mNumberChannels;
    unsigned long p_frames_per_buffer = static_cast<unsigned long>(ioData->mBuffers[0].mDataByteSize / bytesPerFrame);

    float *dst = static_cast<float *>(ioData->mBuffers[0].mData);
    unsigned long current_frame = 0;
    AudioFrame frame{};
    for (; current_frame < p_frames_per_buffer; current_frame++) {
        AudioSource::ReadResult read_result = audio_source->read(frame);
        if (read_result == AudioSource::ReadResult::Read) {
            for (int current_channel = 0; current_channel < audio_source->get_channel_num(); current_channel++) {
                std::clamp(frame[current_channel], AudioFrame::MIN_SAMPLE_VALUE, AudioFrame::MAX_SAMPLE_VALUE);
                *dst++ = (float) frame[current_channel];
            }
        } else if (read_result == AudioSource::ReadResult::End) {
            break;
        } else if (read_result == AudioSource::ReadResult::Pause) {
            break;
        } else if (read_result == AudioSource::ReadResult::Remove) {
            break;
        }
    }

    if (current_frame < p_frames_per_buffer) {
        // fill buffer with silence if not enough samples available.
        unsigned long missing_frames = p_frames_per_buffer - current_frame;
        unsigned long missing_samples = missing_frames * (unsigned long) audio_source->get_channel_num();
        unsigned long current_sample = 0;
        for (; current_sample < missing_samples; current_sample++) {
            *dst++ = 0;
        }
    }

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

    uint32_t input_stream_count = Lowl::Audio::CoreAudioUtilities::get_num_stream(
            p_device_id,
            kAudioDevicePropertyScopeInput,
            error
    );
    if (error.has_error()) {
        return nullptr;
    }
    LOWL_LOG_DEBUG_F("Device:%u - input_stream_count: %d", p_device_id, input_stream_count);

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

    Lowl::SampleRate default_sample_rate = Lowl::Audio::CoreAudioUtilities::get_device_default_sample_rate(
            p_device_id,
            error
    );
    if (error.has_error()) {
        return nullptr;
    }
    LOWL_LOG_DEBUG_F("Device:%u - default_sample_rate: %f", p_device_id, default_sample_rate);

    uint32_t input_channel_count = Lowl::Audio::CoreAudioUtilities::get_num_channel(
            p_device_id,
            kAudioDevicePropertyScopeInput,
            error
    );
    if (error.has_error()) {
        return nullptr;
    }
    LOWL_LOG_DEBUG_F("Device:%u - input_channel_count: %d", p_device_id, input_channel_count);

    uint32_t output_channel_count = Lowl::Audio::CoreAudioUtilities::get_num_channel(
            p_device_id,
            kAudioDevicePropertyScopeOutput,
            error
    );
    if (error.has_error()) {
        return nullptr;
    }
    LOWL_LOG_DEBUG_F("Device:%u - output_channel_count: %d", p_device_id, output_channel_count);

    std::vector<AudioObjectID> output_streams = Lowl::Audio::CoreAudioUtilities::get_stream_ids(
            p_device_id,
            kAudioDevicePropertyScopeOutput,
            error
    );
    if (error.has_error()) {
        return nullptr;
    }
    LOWL_LOG_DEBUG_F("Device:%u - output_streams count: %zu", p_device_id, output_streams.size());

    uint32_t latency_high = Lowl::Audio::CoreAudioUtilities::get_latency_high(
            p_device_id,
            output_streams[0],
            kAudioDevicePropertyScopeOutput,
            error
    );
    if (error.has_error()) {
        return nullptr;
    }
    LOWL_LOG_DEBUG_F("Device:%u - output_streams[0] latency_high: %d", p_device_id, latency_high);

    uint32_t latency_low = Lowl::Audio::CoreAudioUtilities::get_latency_low(
            64,
            p_device_id,
            output_streams[0],
            kAudioDevicePropertyScopeOutput,
            error
    );
    if (error.has_error()) {
        return nullptr;
    }
    LOWL_LOG_DEBUG_F("Device:%u - output_streams[0] latency_low: %d", p_device_id, latency_low);


    std::vector<AudioDeviceProperties> audio_device_properties = create_device_properties(p_device_id);

    std::unique_ptr<CoreAudioDevice> device = std::make_unique<CoreAudioDevice>(_constructor_tag{});
    device->name = "[" + p_driver_name + "] " + device_name;
    device->device_id = p_device_id;
    device->input_stream_count = input_stream_count;
    device->output_stream_count = output_stream_count;
    device->properties = audio_device_properties;
    LOWL_LOG_DEBUG_F("Device:%u - created", p_device_id);

    return device;
}

void Lowl::Audio::CoreAudioDevice::start(AudioDeviceProperties p_audio_device_properties,
                                         std::shared_ptr<AudioSource> p_audio_source,
                                         Error &error) {
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

    SampleCount frames_per_buffer = set_frames_per_buffer(64, error);
    if (error.has_error()) {
        LOWL_LOG_ERROR_F("failed to set_frames_per_buffer (device:%u)", device_id);
        return;
    }

    SampleRate sample_rate = set_sample_rate(audio_source->get_sample_rate(), error);
    if (error.has_error()) {
        LOWL_LOG_ERROR_F("failed to set_sample_rate (device:%u)", device_id);
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

    AudioStreamBasicDescription description = create_description(p_audio_device_properties);
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

    if (p_audio_device_properties.exclusive_mode) {
        pid_t output_hog_pid = CoreAudioUtilities::get_output_hog_pid(device_id, error);
        if (error.has_error()) {
            LOWL_LOG_ERROR_F("Device:%u - failed to check hog status", device_id);
        } else if (output_hog_pid == -1) {
            hog_pid = getpid();
            CoreAudioUtilities::set_output_hog_device_pid(device_id, hog_pid, error);
            if (error.has_error()) {
                LOWL_LOG_ERROR_F("Device:%u - failed to hog", device_id);
                hog_pid = CoreAudioUtilities::freeHogDevice;
            } else {
                LOWL_LOG_DEBUG_F("Device:%u - hogged (hog_pid:%u)", device_id, hog_pid);
            }
        } else {
            LOWL_LOG_ERROR_F("Device:%u - failed to hog (output_hog_pid:%u, getpid():%u)",
                             device_id, output_hog_pid, getpid()
            );
        }
    }

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

Lowl::SampleCount Lowl::Audio::CoreAudioDevice::set_frames_per_buffer(
        SampleCount p_frames_per_buffer, Lowl::Error &error) {

    SampleCount requested_frames_per_buffer = p_frames_per_buffer;
    SampleCount actual_frames_per_buffer = 0;
    CoreAudioUtilities::set_buffer_frame_size(
            device_id,
            kAudioDevicePropertyScopeOutput,
            requested_frames_per_buffer,
            error
    );
    actual_frames_per_buffer = CoreAudioUtilities::get_buffer_frame_size(
            device_id,
            kAudioDevicePropertyScopeOutput,
            error
    );

    // Did we get the size we asked for?
    if (actual_frames_per_buffer != requested_frames_per_buffer) {
        AudioValueRange range = CoreAudioUtilities::get_buffer_frame_size_range(
                device_id,
                kAudioDevicePropertyScopeOutput,
                error
        );
        if (requested_frames_per_buffer < range.mMinimum) {
            requested_frames_per_buffer = static_cast<UInt32>(range.mMinimum);
        } else if (requested_frames_per_buffer > range.mMaximum) {
            requested_frames_per_buffer = static_cast<UInt32>(range.mMaximum);
        }
        CoreAudioUtilities::set_buffer_frame_size(
                device_id,
                kAudioDevicePropertyScopeOutput,
                requested_frames_per_buffer,
                error
        );
        actual_frames_per_buffer = CoreAudioUtilities::get_buffer_frame_size(
                device_id,
                kAudioDevicePropertyScopeOutput,
                error
        );
    }
    return actual_frames_per_buffer;
}

Lowl::SampleRate Lowl::Audio::CoreAudioDevice::set_sample_rate(SampleRate p_sample_rate, Lowl::Error &error) {
    return 0;
}

Lowl::Audio::CoreAudioDevice::CoreAudioDevice(_constructor_tag ct) : AudioDevice(ct) {
    audio_unit = nullptr;
}

Lowl::Audio::CoreAudioDevice::~CoreAudioDevice() {
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


    Error error;
    AudioUnit _Nullable test_audio_unit = create_audio_unit(p_device_id, error);


    std::vector<Lowl::Audio::AudioDeviceProperties> properties_list = std::vector<Lowl::Audio::AudioDeviceProperties>();

    // device default properties
    //AudioDeviceProperties default_properties = to_audio_device_properties(p_wave_format);
    //std::vector<Lowl::Audio::AudioDeviceProperties> default_properties_list = create_device_properties(
    //        p_wasapi_device,
    //        default_properties,
    //        device_name,
    //        error
    //);
    //properties_list.insert(properties_list.end(), default_properties_list.begin(), default_properties_list.end());

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

            test_properties.exclusive_mode = true;


            AudioStreamBasicDescription description = create_description(test_properties);
            OSStatus result = AudioUnitSetProperty(
                    test_audio_unit,
                    kAudioUnitProperty_StreamFormat,
                    kAudioUnitScope_Input,
                    CoreAudioUtilities::kOutputBus,
                    &description,
                    sizeof(AudioStreamBasicDescription)
            );
            if (result != noErr) {
                LOWL_LOG_ERROR_F("failed to set AudioStreamBasicDescription (device:%u, OSStatus:%u)", p_device_id,
                                 result);
                error.set_vendor_error(result, Error::VendorError::CoreAudioVendorError);
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
    description.mFormatFlags = kAudioFormatFlagsNativeFloatPacked;
    description.mFramesPerPacket = 1;
    description.mBitsPerChannel = sizeof(float) * 8;
    description.mSampleRate = p_device_properties.sample_rate;
    description.mBytesPerPacket = static_cast<UInt32>(sizeof(float) * channel_num);
    description.mBytesPerFrame = static_cast<UInt32>(sizeof(float) * channel_num);
    description.mChannelsPerFrame = static_cast<UInt32>(channel_num);
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