#ifdef LOWL_DRIVER_CORE_AUDIO

#include "lowl_audio_core_audio_device.h"

#include "lowl_logger.h"

#include "audio/coreaudio/lowl_audio_core_audio_utilities.h"

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

    // result = AudioComponentInstanceDispose( *audioUnit );
    // if (result != noErr) {
    //     error.set_error(ErrorCode::Error);
    //     return;
    // }

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
    AudioStreamBasicDescription desiredFormat;
    desiredFormat.mFormatID = kAudioFormatLinearPCM;
    desiredFormat.mFormatFlags = kAudioFormatFlagsNativeFloatPacked;
    desiredFormat.mFramesPerPacket = 1;
    desiredFormat.mBitsPerChannel = sizeof(float) * 8;

    // if( outStreamParams && !inStreamParams ) {
    /*The callback never calls back if we don't set the FPB */
    /*This seems weird, because I would think setting anything on the device
      would be disruptive.*/


    SampleCount requested_frames_per_buffer = 64;
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



    ///    paResult = setBestFramesPerBuffer( *audioDevice, TRUE,
    ///                                       requestedFramesPerBuffer,
    ///                                       actualOutputFramesPerBuffer );
    ///    if( paResult ) goto error;
    ///    if( macOutputStreamFlags & paMacCoreChangeDeviceParameters ) {
    ///        bool requireExact;
    ///        requireExact=macOutputStreamFlags & paMacCoreFailIfConversionRequired;
    ///        paResult = setBestSampleRateForDevice( *audioDevice, TRUE,
    ///                                               requireExact, sampleRate );
    ///        if( paResult ) goto error;
    ///    }
    //  }

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

    /* now set the format on the Audio Units. */
    //if( outStreamParams )
    //{
    //    desiredFormat.mSampleRate    =sampleRate;
    //    desiredFormat.mBytesPerPacket=sizeof(float)*outStreamParams->channelCount;
    //    desiredFormat.mBytesPerFrame =sizeof(float)*outStreamParams->channelCount;
    //    desiredFormat.mChannelsPerFrame = outStreamParams->channelCount;
    //    ERR_WRAP( AudioUnitSetProperty( *audioUnit,
    //                                    kAudioUnitProperty_StreamFormat,
    //                                    kAudioUnitScope_Input,
    //                                    OUTPUT_ELEMENT,
    //                                    &desiredFormat,
    //                                    sizeof(AudioStreamBasicDescription) ) );
    //}


    /* set the maximumFramesPerSlice */
    /* not doing this causes real problems
       (eg. the callback might not be called). The idea of setting both this
       and the frames per buffer on the device is that we'll be most likely
       to actually get the frame size we requested in the callback with the
       minimum latency. */
//   if( outStreamParams ) {
//       UInt32 size = sizeof( *actualOutputFramesPerBuffer );
//       ERR_WRAP( AudioUnitSetProperty( *audioUnit,
//                                       kAudioUnitProperty_MaximumFramesPerSlice,
//                                       kAudioUnitScope_Input,
//                                       OUTPUT_ELEMENT,
//                                       actualOutputFramesPerBuffer,
//                                       sizeof(*actualOutputFramesPerBuffer) ) );
//       ERR_WRAP( AudioUnitGetProperty( *audioUnit,
//                                       kAudioUnitProperty_MaximumFramesPerSlice,
//                                       kAudioUnitScope_Global,
//                                       OUTPUT_ELEMENT,
//                                       actualOutputFramesPerBuffer,
//                                       &size ) );
//   }



    /* -- set IOProc (callback) -- */
    //  callbackKey = outStreamParams ? kAudioUnitProperty_SetRenderCallback
    //                                : kAudioOutputUnitProperty_SetInputCallback ;
    //  rcbs.inputProc = AudioIOProc;
    //  rcbs.inputProcRefCon = refCon;
    //  ERR_WRAP( AudioUnitSetProperty( *audioUnit,
    //                                  callbackKey,
    //                                  kAudioUnitScope_Output,
    //                                  outStreamParams ? OUTPUT_ELEMENT : INPUT_ELEMENT,
    //                                  &rcbs,
    //                                  sizeof(rcbs)) );


    /* initialize the audio unit */
    result = AudioUnitInitialize(audio_unit);
    if (result != noErr) {
        error.set_error(ErrorCode::Error);
        return;
    }
    //VDBUG( ("Opened device %ld for output.\n", (long)*audioDevice ) );
}

void Lowl::Audio::CoreAudioDevice::stop(Lowl::Error &error) {

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