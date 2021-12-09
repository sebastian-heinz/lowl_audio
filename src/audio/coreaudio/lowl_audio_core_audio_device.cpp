#ifdef LOWL_DRIVER_CORE_AUDIO

#include "lowl_audio_core_audio_device.h"

#include "lowl_logger.h"

#include "audio/coreaudio/lowl_audio_core_audio_utilities.h"

static OSStatus audio_callback(
        void *inRefCon,
        AudioUnitRenderActionFlags *ioActionFlags,
        const AudioTimeStamp *inTimeStamp,
        UInt32 inBusNumber,
        UInt32 inNumberFrames,
        AudioBufferList *__nullable ioData
) {
    Lowl::Audio::CoreAudioDevice *device = (Lowl::Audio::CoreAudioDevice *) inRefCon;
    return device->callback(ioActionFlags, inTimeStamp, inBusNumber, inNumberFrames, ioData);
}

OSStatus Lowl::Audio::CoreAudioDevice::callback(
        AudioUnitRenderActionFlags *ioActionFlags,
        const AudioTimeStamp *inTimeStamp,
        UInt32 inBusNumber,
        UInt32 inNumberFrames,
        AudioBufferList *ioData
) {
    OSStatus result = noErr;
    //  AudioBufferList buffers;

    long bytesPerFrame = sizeof(float) * ioData->mBuffers[0].mNumberChannels;
    //assert( ioData->mNumberBuffers == 1 );
    //assert( ioData->mBuffers[0].mNumberChannels == stream->userOutChan );
    unsigned long p_frames_per_buffer = static_cast<unsigned long>(ioData->mBuffers[0].mDataByteSize / bytesPerFrame);

  //  uint p_frames_per_buffer = ioData->mBuffers[0].mDataByteSize;

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


    // result = AudioUnitRender(
    //         audio_unit,
    //         ioActionFlags,
    //         inTimeStamp,
    //         CoreAudioUtilities::kInputBus,
    //         inNumberFrames,
    //         ioData
    // );
    // if (result != noErr) {
    //     return result;
    // }
    return noErr;
}

std::unique_ptr<Lowl::Audio::CoreAudioDevice>
Lowl::Audio::CoreAudioDevice::construct(const std::string &p_driver_name, AudioObjectID p_device_id, Error &error) {
    LOWL_LOG_DEBUG_F("Device:%u - creating", p_device_id);

    std::string device_name = Lowl::Audio::CoreAudioUtilities::get_device_name(p_device_id, error);
    if (error.has_error()) {
        return nullptr;
    }
    LOWL_LOG_DEBUG_F("Device:%u - name: %s", p_device_id, device_name.c_str());

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
        // has no output
        error.set_error(ErrorCode::Error);
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

    std::unique_ptr<CoreAudioDevice> device = std::unique_ptr<CoreAudioDevice>(new CoreAudioDevice());
    device->set_name("[" + p_driver_name + "] " + device_name);
    device->device_id = p_device_id;
    device->input_stream_count = input_stream_count;
    device->output_stream_count = output_stream_count;
    LOWL_LOG_DEBUG_F("Device:%u - created", p_device_id);

    return device;
}

void Lowl::Audio::CoreAudioDevice::start(std::shared_ptr<AudioSource> p_audio_source, Lowl::Error &error) {
    audio_source = p_audio_source;

    AudioComponentDescription desc;
    desc.componentType = kAudioUnitType_Output;
    desc.componentSubType = kAudioUnitSubType_HALOutput;
    desc.componentManufacturer = kAudioUnitManufacturer_Apple;
    desc.componentFlags = 0;
    desc.componentFlagsMask = 0;

    AudioComponent comp = AudioComponentFindNext(nullptr, &desc);
    if (!comp) {
        error.set_error(ErrorCode::Error);
        return;
    }

    OSStatus result = noErr;
    result = AudioComponentInstanceNew(comp, &audio_unit);
    if (result != noErr) {
        error.set_error(ErrorCode::Error);
        return;
    }

    /// TODO err
    // result = AudioComponentInstanceDispose( *audioUnit );
    // if (result != noErr) {
    //     error.set_error(ErrorCode::Error);
    //     return;
    // }
    ///

    /* -- add listener for dropouts -- */
    // result = PaMacCore_AudioDeviceAddPropertyListener( *audioDevice,
    //                                                    0,
    //                                                    outStreamParams ? false : true,
    //                                                    kAudioDeviceProcessorOverload,
    //                                                    xrunCallback,
    //                                                    addToXRunListenerList( (void *)stream ) ) ;
    // if( result == kAudioHardwareIllegalOperationError ) {
    //     // -- already registered, we're good
    // } else {
    //     // -- not already registered, just check for errors
    //     ERR_WRAP( result );
    // }

    /* -- listen for stream start and stop -- */
    //  ERR_WRAP( AudioUnitAddPropertyListener( *audioUnit,
    //                                          kAudioOutputUnitProperty_IsRunning,
    //                                          startStopCallback,
    //                                          (void *)stream ) );
    //


    // set preferred frames
    SampleCount frames_per_buffer = set_frames_per_buffer(64, error);
    if (error.has_error()) {
        return;
    }

    // paResult = setBestSampleRateForDevice( *audioDevice, TRUE,

    //  /* -- set the quality of the output converter -- */
    //  if( outStreamParams ) {
    //      UInt32 value = kAudioConverterQuality_Max;
    //      switch( macOutputStreamFlags & 0x0700 ) {
    //          case 0x0100: /*paMacCore_ConversionQualityMin:*/
    //              value=kRenderQuality_Min;
    //              break;
    //          case 0x0200: /*paMacCore_ConversionQualityLow:*/
    //              value=kRenderQuality_Low;
    //              break;
    //          case 0x0300: /*paMacCore_ConversionQualityMedium:*/
    //              value=kRenderQuality_Medium;
    //              break;
    //          case 0x0400: /*paMacCore_ConversionQualityHigh:*/
    //              value=kRenderQuality_High;
    //              break;
    //      }
    //      ERR_WRAP( AudioUnitSetProperty( *audioUnit,
    //                                      kAudioUnitProperty_RenderQuality,
    //                                      kAudioUnitScope_Global,
    //                                      OUTPUT_ELEMENT,
    //                                      &value,
    //                                      sizeof(value) ) );
    //  }

    // set format
    AudioStreamBasicDescription desiredFormat;
    desiredFormat.mFormatID = kAudioFormatLinearPCM;
    desiredFormat.mFormatFlags = kAudioFormatFlagsNativeFloatPacked;
    desiredFormat.mFramesPerPacket = 1;
    desiredFormat.mBitsPerChannel = sizeof(float) * 8;
    desiredFormat.mSampleRate = audio_source->get_sample_rate();
    desiredFormat.mBytesPerPacket = static_cast<UInt32>(sizeof(float) * audio_source->get_channel_num());
    desiredFormat.mBytesPerFrame = static_cast<UInt32>(sizeof(float) * audio_source->get_channel_num());
    desiredFormat.mChannelsPerFrame = static_cast<UInt32>(audio_source->get_channel_num());
    result = AudioUnitSetProperty(
            audio_unit,
            kAudioUnitProperty_StreamFormat,
            kAudioUnitScope_Input,
            CoreAudioUtilities::kOutputBus,
            &desiredFormat,
            sizeof(AudioStreamBasicDescription)
    );
    if (result != noErr) {
        error.set_error(ErrorCode::Error);
        return;
    }


    // set max frames
    CoreAudioUtilities::set_maximum_frames_per_slice(
            audio_unit,
            kAudioUnitScope_Input,
            CoreAudioUtilities::kOutputBus,
            frames_per_buffer,
            error
    );
    if (error.has_error()) {
        return;
    }
    SampleCount max_frames_per_buffer = CoreAudioUtilities::get_maximum_frames_per_slice(
            audio_unit,
            kAudioUnitScope_Global,
            CoreAudioUtilities::kOutputBus,
            error
    );
    if (error.has_error()) {
        return;
    }

    // set audio callback
    AURenderCallbackStruct render_callback;
    render_callback.inputProc = &audio_callback;
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
        error.set_error(ErrorCode::Error);
        return;
    }

    // initialize
    result = AudioUnitInitialize(audio_unit);
    if (result != noErr) {
        error.set_error(ErrorCode::Error);
        return;
    }

    // start
    result = AudioOutputUnitStart(audio_unit);
    if (result != noErr) {
        error.set_error(ErrorCode::Error);
        return;
    }

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

void Lowl::Audio::CoreAudioDevice::stop(Lowl::Error &error) {
    OSStatus result = noErr;
    result = AudioOutputUnitStop(audio_unit);
    //result = BlockWhileAudioUnitIsRunning(audio_unit, 0);
    result = AudioUnitReset(audio_unit, kAudioUnitScope_Global, 0);
}

bool Lowl::Audio::CoreAudioDevice::is_supported(AudioChannel channel, Lowl::SampleRate sample_rate,
                                                SampleFormat sample_format, Lowl::Error &error) {
    return false;
}

Lowl::SampleRate Lowl::Audio::CoreAudioDevice::get_default_sample_rate() {
    return 0;
}

void Lowl::Audio::CoreAudioDevice::set_exclusive_mode(bool p_exclusive_mode, Lowl::Error &error) {

}

Lowl::Audio::CoreAudioDevice::CoreAudioDevice() : AudioDevice() {
    audio_unit = nullptr;
}

Lowl::Audio::CoreAudioDevice::~CoreAudioDevice() {

}


#endif /* LOWL_DRIVER_CORE_AUDIO */