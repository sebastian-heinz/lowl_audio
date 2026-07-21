#ifdef LOWL_DRIVER_CORE_AUDIO

#include "lowl_audio_core_audio_device.h"

#include <algorithm>
#include <cstring>

#include "audio/backend/coreaudio/lowl_audio_core_audio_layout.h"
#include "audio/backend/coreaudio/lowl_audio_core_audio_utilities.h"
#include "audio/lowl_audio_setting.h"
#include "audio/lowl_audio_utilities.h"
#include "lowl_logger.h"

namespace {
    using Lowl::Audio::AudioDeviceProperties;
    using Lowl::Audio::AudioBlockView;
    using Lowl::Audio::ChannelLayout;

    bool is_layout_required(const AudioDeviceProperties &p_properties) {
        const ChannelLayout &layout = p_properties.audio_format.channel_layout;
        return layout.is_valid() && layout.channel_count > 2;
    }

    bool set_audio_unit_channel_layout(AudioUnit p_audio_unit,
                                       AudioUnitScope p_scope,
                                       AudioUnitElement p_element,
                                       const AudioDeviceProperties &p_properties,
                                       Lowl::Error &error) {
        const std::vector<uint8_t> layout_data =
            Lowl::Audio::CoreAudioLayout::create_channel_layout_data(p_properties.audio_format.channel_layout);
        if (layout_data.empty()) {
            return true;
        }

        Lowl::Audio::CoreAudioUtilities::set_audio_unit_channel_layout(
            p_audio_unit, p_scope, p_element, layout_data.data(), static_cast<UInt32>(layout_data.size()), error);
        if (!error.has_error()) {
            return true;
        }

        const long vendor_error = error.get_vendor_error();
        const bool unsupported_property = vendor_error == kAudioUnitErr_InvalidProperty ||
                                          vendor_error == kAudioUnitErr_PropertyNotWritable ||
                                          vendor_error == kAudioUnitErr_InvalidElement;
        if (unsupported_property && !is_layout_required(p_properties)) {
            error.clear();
            return true;
        }
        return false;
    }

    void set_error_if_clear(Lowl::Error &error, OSStatus result) {
        if (!error.has_error()) {
            error.set_vendor_error(result, Lowl::Error::VendorError::CoreAudioVendorError);
        }
    }

    void dispose_audio_unit(AudioUnit &p_audio_unit, Lowl::Error *p_error = nullptr) {
        if (p_audio_unit == nullptr) {
            return;
        }
        const OSStatus result = AudioComponentInstanceDispose(p_audio_unit);
        if (result != noErr && p_error != nullptr) {
            set_error_if_clear(*p_error, result);
        }
        p_audio_unit = nullptr;
    }

} // namespace

static OSStatus osx_audio_callback(void *inRefCon,
                                   AudioUnitRenderActionFlags *ioActionFlags,
                                   const AudioTimeStamp *inTimeStamp,
                                   UInt32 inBusNumber,
                                   UInt32 inNumberFrames,
                                   AudioBufferList *_Nullable ioData) {
    Lowl::Audio::CoreAudioDevice *device = (Lowl::Audio::CoreAudioDevice *)inRefCon;
    return device->audio_callback(ioActionFlags, inTimeStamp, inBusNumber, inNumberFrames, ioData);
}

static void osx_start_stop_callback(
    void *inRefCon, AudioUnit inUnit, AudioUnitPropertyID inID, AudioUnitScope inScope, AudioUnitElement inElement) {
    Lowl::Audio::CoreAudioDevice *device = (Lowl::Audio::CoreAudioDevice *)inRefCon;
    device->start_stop_callback(inUnit, inID, inScope, inElement);
}

OSStatus osx_property_callback(AudioObjectID inObjectID,
                               UInt32 inNumberAddresses,
                               const AudioObjectPropertyAddress *inAddresses,
                               void *_Nullable inClientData) {
    Lowl::Audio::CoreAudioDevice *device = (Lowl::Audio::CoreAudioDevice *)inClientData;
    return device->property_callback(inObjectID, inNumberAddresses, inAddresses);
}

OSStatus Lowl::Audio::CoreAudioDevice::audio_callback(AudioUnitRenderActionFlags *ioActionFlags,
                                                      const AudioTimeStamp *inTimeStamp,
                                                      UInt32 inBusNumber,
                                                      UInt32 inNumberFrames,
                                                      AudioBufferList *ioData) {
    if (ioData == nullptr || ioData->mNumberBuffers == 0) {
        return noErr;
    }
    RenderState *const published_state = load_render_state();
    if (published_state == nullptr || !published_state->audio_source) {
        for (UInt32 buffer_index = 0; buffer_index < ioData->mNumberBuffers; buffer_index++) {
            ::AudioBuffer &buffer = ioData->mBuffers[buffer_index];
            if (buffer.mData != nullptr) {
                std::memset(buffer.mData, 0, buffer.mDataByteSize);
            }
        }
        return noErr;
    }

    const AudioDeviceProperties &published_properties = published_state->audio_device_properties;
    const uint32_l sample_size_bytes =
        static_cast<uint32_l>(get_sample_size_bytes(published_properties.sample_format));
    const uint32_l channels =
        static_cast<uint32_l>(published_properties.audio_format.channel_layout.channel_count);
    const bool can_render_non_interleaved_float32 =
        published_properties.sample_format == Lowl::Audio::SampleFormat::FLOAT_32 && ioData->mNumberBuffers == channels;

    if (can_render_non_interleaved_float32) {
        AudioBlockView output_block{};
        output_block.frame_count = static_cast<uint32_t>(inNumberFrames);
        output_block.channel_count = static_cast<uint8_t>(channels);

        const uint32_l bytes_per_channel = inNumberFrames * sample_size_bytes;
        for (uint32_l channel_index = 0; channel_index < channels; channel_index++) {
            if (ioData->mBuffers[channel_index].mData == nullptr ||
                ioData->mBuffers[channel_index].mDataByteSize < bytes_per_channel) {
                return kAudio_ParamError;
            }
            std::memset(ioData->mBuffers[channel_index].mData, 0, bytes_per_channel);
            output_block.channels[static_cast<size_t>(channel_index)] =
                static_cast<Lowl::Sample *>(ioData->mBuffers[channel_index].mData);
        }

        Lowl::Audio::AudioSource::MixGainVector unity_gain;
        published_state->audio_source->mix_into(output_block, unity_gain);
        return noErr;
    }

    const uint32_l bytes_per_frame = sample_size_bytes * channels;

    const uint32_l actual_bytes_needed = inNumberFrames * bytes_per_frame;
    if (actual_bytes_needed > ioData->mBuffers[0].mDataByteSize) {
        // Handle error: buffer provided by system is too small for requested frames
        return kAudio_ParamError;
    }

    void *dst = ioData->mBuffers[0].mData;
    render_to_device_buffer(published_state, dst, ioData->mBuffers[0].mDataByteSize, inNumberFrames, bytes_per_frame);
    return noErr;
}

void Lowl::Audio::CoreAudioDevice::start_stop_callback(AudioUnit inUnit,
                                                       AudioUnitPropertyID inID,
                                                       AudioUnitScope inScope,
                                                       AudioUnitElement inElement) {
}

OSStatus Lowl::Audio::CoreAudioDevice::property_callback(AudioObjectID inObjectID,
                                                         UInt32 inNumberAddresses,
                                                         const AudioObjectPropertyAddress *inAddresses) {
    (void)inObjectID;
    if (inNumberAddresses == 0 || inAddresses == nullptr) {
        return noErr;
    }

    for (UInt32 address_index = 0; address_index < inNumberAddresses; address_index++) {
        handle_property_address(inAddresses[address_index]);
    }
    return noErr;
}

void Lowl::Audio::CoreAudioDevice::handle_property_address(const AudioObjectPropertyAddress &p_address) {
    switch (p_address.mSelector) {
        case kAudioDeviceProcessorOverload:
            break;
        default:
            break;
    }
}

Lowl::Audio::CoreAudioDevice::CoreAudioDevice(_constructor_tag ct) : AudioDevice(ct) {
    device_id = 0;
    audio_unit = nullptr;
    hog_pid = -1;
    device_property_listener_registered = false;
    running_listener_registered = false;
    audio_unit_initialized = false;
}

std::unique_ptr<Lowl::Audio::CoreAudioDevice>
Lowl::Audio::CoreAudioDevice::construct(const std::string &p_driver_name, AudioObjectID p_device_id, Error &error) {
    LOWL_LOG_DEBUG_F("construct->%u - enter", p_device_id);

    std::string device_name = Lowl::Audio::CoreAudioUtilities::get_device_name(p_device_id, error);
    if (error.has_error()) {
        LOWL_LOG_DEBUG_F("construct->%u - get_device_name::FAILED", p_device_id);
        return nullptr;
    }
    LOWL_LOG_DEBUG_F("construct->%u (%s) - get_device_name::OK", p_device_id, device_name.c_str());

    uint32_t output_stream_count =
        Lowl::Audio::CoreAudioUtilities::get_num_stream(p_device_id, kAudioDevicePropertyScopeOutput, error);
    if (error.has_error()) {
        return nullptr;
    }
    LOWL_LOG_DEBUG_F("Device:%u - output_stream_count: %d", p_device_id, output_stream_count);
    if (output_stream_count <= 0) {
        error.set_error(ErrorCode::NoAudioOutput);
        return nullptr;
    }

    std::vector<AudioObjectID> output_streams =
        Lowl::Audio::CoreAudioUtilities::get_stream_ids(p_device_id, kAudioDevicePropertyScopeOutput, error);
    if (error.has_error()) {
        return nullptr;
    }
    LOWL_LOG_DEBUG_F("Device:%u - output_streams count: %zu", p_device_id, output_streams.size());

    std::vector<AudioDeviceProperties> audio_device_properties_list = create_device_properties(p_device_id);

    std::unique_ptr<CoreAudioDevice> device = std::make_unique<CoreAudioDevice>(_constructor_tag{});
    device->name = "[" + p_driver_name + "] " + device_name;
    device->device_id = p_device_id;
    device->properties_list = audio_device_properties_list;
    LOWL_LOG_DEBUG_F("Device:%u - created", p_device_id);

    return device;
}

void Lowl::Audio::CoreAudioDevice::start(AudioDeviceProperties p_audio_device_properties,
                                         std::shared_ptr<AudioSource> p_audio_source,
                                         Error &error) {
    Error stop_error;
    stop(stop_error);
    if (stop_error.has_error()) {
        LOWL_LOG_ERROR_F("CoreAudioDevice::start cleanup failed before restart (device:%u)", device_id);
    }

    if (!p_audio_device_properties.is_supported) {
        error.set_error(Lowl::ErrorCode::DevicePropertiesNotSupported);
        return;
    }
    if (p_audio_source && p_audio_source->get_audio_format() != p_audio_device_properties.get_audio_format()) {
        LOWL_LOG_ERROR("CoreAudioDevice::start: source AudioFormat does not match the device AudioFormat.");
        error.set_error(Lowl::ErrorCode::InvalidParameter);
        return;
    }

    AudioStreamBasicDescription description{};
    if (!create_description(p_audio_device_properties, description, error)) {
        LOWL_LOG_ERROR("CoreAudioDevice::start: failed to create stream description for properties " +
                       p_audio_device_properties.to_string() + ".");
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
        device_id, kAudioDeviceProcessorOverload, kAudioDevicePropertyScopeOutput, osx_property_callback, this, error);
    if (error.has_error()) {
        LOWL_LOG_ERROR_F("failed to add property listener (device:%u)", device_id);
        cleanup_failed_start();
        return;
    }
    device_property_listener_registered = true;

    OSStatus result =
        AudioUnitAddPropertyListener(audio_unit, kAudioOutputUnitProperty_IsRunning, &osx_start_stop_callback, this);
    if (result != noErr) {
        LOWL_LOG_ERROR_F("failed to add isRunning listener (device:%u, OSStatus:%u)", device_id, result);
        error.set_vendor_error(result, Error::VendorError::CoreAudioVendorError);
        cleanup_failed_start();
        return;
    }
    running_listener_registered = true;

    SampleCount frames_per_buffer = CoreAudioUtilities::set_frames_per_buffer(device_id, 64, error);
    if (error.has_error()) {
        LOWL_LOG_ERROR_F("failed to set_frames_per_buffer (device:%u)", device_id);
        cleanup_failed_start();
        return;
    }

    CoreAudioUtilities::set_render_quality(
        audio_unit, kAudioUnitScope_Global, CoreAudioUtilities::kOutputBus, kRenderQuality_High, error);
    if (error.has_error()) {
        LOWL_LOG_ERROR_F("failed to set_render_quality (device:%u)", device_id);
        cleanup_failed_start();
        return;
    }

    result = AudioUnitSetProperty(audio_unit,
                                  kAudioUnitProperty_StreamFormat,
                                  kAudioUnitScope_Input,
                                  CoreAudioUtilities::kOutputBus,
                                  &description,
                                  sizeof(AudioStreamBasicDescription));
    if (result != noErr) {
        LOWL_LOG_ERROR_F("failed to set AudioStreamBasicDescription (device:%u, OSStatus:%u)", device_id, result);
        error.set_vendor_error(result, Error::VendorError::CoreAudioVendorError);
        cleanup_failed_start();
        return;
    }

    if (!set_audio_unit_channel_layout(
            audio_unit, kAudioUnitScope_Input, CoreAudioUtilities::kOutputBus, audio_device_properties, error)) {
        LOWL_LOG_ERROR_F("failed to set AudioChannelLayout (device:%u)", device_id);
        cleanup_failed_start();
        return;
    }

    CoreAudioUtilities::set_maximum_frames_per_slice(
        audio_unit, kAudioUnitScope_Input, CoreAudioUtilities::kOutputBus, frames_per_buffer, error);
    if (error.has_error()) {
        LOWL_LOG_ERROR_F("failed to set_maximum_frames_per_slice (device:%u)", device_id);
        cleanup_failed_start();
        return;
    }

    SampleCount max_frames_per_buffer = CoreAudioUtilities::get_maximum_frames_per_slice(
        audio_unit, kAudioUnitScope_Global, CoreAudioUtilities::kOutputBus, error);
    if (error.has_error()) {
        LOWL_LOG_ERROR_F("failed to get_maximum_frames_per_slice (device:%u)", device_id);
        cleanup_failed_start();
        return;
    }

    if (!allocate_render_buffer(static_cast<unsigned long>(max_frames_per_buffer))) {
        error.set_error(ErrorCode::InvalidOperationWhileActive);
        cleanup_failed_start();
        return;
    }

    AURenderCallbackStruct render_callback;
    render_callback.inputProc = &osx_audio_callback;
    render_callback.inputProcRefCon = this;
    result = AudioUnitSetProperty(audio_unit,
                                  kAudioUnitProperty_SetRenderCallback,
                                  kAudioUnitScope_Output,
                                  CoreAudioUtilities::kOutputBus,
                                  &render_callback,
                                  sizeof(render_callback));
    if (result != noErr) {
        LOWL_LOG_ERROR_F("failed to set render callback (device:%u, OSStatus:%u)", device_id, result);
        error.set_vendor_error(result, Error::VendorError::CoreAudioVendorError);
        cleanup_failed_start();
        return;
    }

    result = AudioUnitInitialize(audio_unit);
    if (result != noErr) {
        LOWL_LOG_ERROR_F("failed at AudioUnitInitialize (device:%u, OSStatus:%u)", device_id, result);
        error.set_vendor_error(result, Error::VendorError::CoreAudioVendorError);
        cleanup_failed_start();
        return;
    }
    audio_unit_initialized = true;

    result = AudioOutputUnitStart(audio_unit);
    if (result != noErr) {
        LOWL_LOG_ERROR_F("failed at AudioOutputUnitStart (device:%u, OSStatus:%u)", device_id, result);
        error.set_vendor_error(result, Error::VendorError::CoreAudioVendorError);
        cleanup_failed_start();
        return;
    }
}

void Lowl::Audio::CoreAudioDevice::stop(Lowl::Error &error) {
    cleanup_audio_unit(error);
}

Lowl::Audio::CoreAudioDevice::~CoreAudioDevice() {
    Error error;
    stop(error);
    if (error.has_error()) {
        LOWL_LOG_ERROR_F("CoreAudioDevice::~CoreAudioDevice cleanup failed (device:%u)", device_id);
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
    auto cleanup_test_audio_unit = [&]() { dispose_audio_unit(test_audio_unit); };

    Lowl::SampleRate default_sample_rate =
        Lowl::Audio::CoreAudioUtilities::get_device_default_sample_rate(p_device_id, error);
    if (error.has_error()) {
        cleanup_test_audio_unit();
        return properties_list;
    }
    LOWL_LOG_DEBUG_F("Device:%u - default_sample_rate: %f", p_device_id, default_sample_rate);

    uint32_t output_channel_count =
        Lowl::Audio::CoreAudioUtilities::get_num_channel(p_device_id, kAudioDevicePropertyScopeOutput, error);
    if (error.has_error()) {
        cleanup_test_audio_unit();
        return properties_list;
    }
    LOWL_LOG_DEBUG_F("Device:%u - output_channel_count: %d", p_device_id, output_channel_count);

    Error layout_error;
    ChannelLayout output_channel_layout =
        Lowl::Audio::CoreAudioUtilities::get_channel_layout(p_device_id, kAudioDevicePropertyScopeOutput, layout_error);
    if (layout_error.has_error()) {
        layout_error.clear();
    }
    if (!output_channel_layout.is_valid()) {
        output_channel_layout = ChannelLayout::from_count(static_cast<uint8_t>(output_channel_count));
        LOWL_LOG_DEBUG_F("Device:%u - output_channel_layout: invalid channel layout, deriving from channel count",
                         p_device_id);
    } else {
        LOWL_LOG_DEBUG_F(
            "Device:%u - output_channel_layout: %s", p_device_id, output_channel_layout.to_string().c_str());
    }

    AudioDeviceProperties default_properties = AudioDeviceProperties();
    default_properties.audio_format = AudioFormat{default_sample_rate, output_channel_layout};
    default_properties.sample_format = SampleFormat::FLOAT_32;

    if (test_device_properties(p_device_id, test_audio_unit, default_properties)) {
        default_properties.is_supported = true;
        properties_list.push_back(default_properties);
    } else {
        LOWL_LOG_ERROR_F(
            "Device:%u - default properties failed test (%s)", p_device_id, default_properties.to_string().c_str());
    }

    // test other capabilities
    std::vector<double> test_sample_rates = Lowl::Audio::AudioSetting::get_test_sample_rates();
    std::vector<SampleFormat> test_sample_formats = Lowl::Audio::AudioSetting::get_test_sample_formats();
    std::vector<ChannelLayout> test_channel_layouts = Lowl::Audio::AudioSetting::get_test_channel_layouts();
    for (unsigned long sample_format_index = 0; sample_format_index < test_sample_formats.size();
         sample_format_index++) {
        for (unsigned long sample_rate_index = 0; sample_rate_index < test_sample_rates.size(); sample_rate_index++) {
            for (const ChannelLayout &probe_layout : test_channel_layouts) {
                AudioDeviceProperties test_properties = AudioDeviceProperties();
                test_properties.sample_format = test_sample_formats[sample_format_index];
                test_properties.audio_format = AudioFormat{test_sample_rates[sample_rate_index], probe_layout};
                test_properties.is_supported = true;

                if (!test_device_properties(p_device_id, test_audio_unit, test_properties)) {
                    continue;
                }

                test_properties.is_supported = true;
                properties_list.push_back(test_properties);
            }
        }
    }

    std::sort(properties_list.begin(), properties_list.end());
    properties_list.erase(std::unique(properties_list.begin(), properties_list.end()), properties_list.end());

    cleanup_test_audio_unit();
    return properties_list;
}

bool Lowl::Audio::CoreAudioDevice::test_device_properties(AudioObjectID p_device_id,
                                                          AudioUnit p_audio_unit,
                                                          AudioDeviceProperties p_properties,
                                                          bool silent) {
    if (!p_properties.audio_format.channel_layout.is_valid()) {
        return false;
    }

    AudioStreamBasicDescription description{};
    Error description_error;
    if (!create_description(p_properties, description, description_error)) {
        if (!silent) {
            LOWL_LOG_ERROR("failed to create AudioStreamBasicDescription (device:" + std::to_string(p_device_id) +
                           ", properties:" + p_properties.to_string() +
                           ", error:" + description_error.get_error_text() + ")");
        }
        return false;
    }
    OSStatus result = AudioUnitSetProperty(p_audio_unit,
                                           kAudioUnitProperty_StreamFormat,
                                           kAudioUnitScope_Input,
                                           CoreAudioUtilities::kOutputBus,
                                           &description,
                                           sizeof(AudioStreamBasicDescription));
    if (result != noErr) {
        if (!silent) {
            LOWL_LOG_ERROR_F("failed to set AudioStreamBasicDescription (device:%u, OSStatus:%u)", p_device_id, result);
        }
        return false;
    }

    Error layout_error;
    if (!set_audio_unit_channel_layout(
            p_audio_unit, kAudioUnitScope_Input, CoreAudioUtilities::kOutputBus, p_properties, layout_error)) {
        if (layout_error.has_error() && !silent) {
            LOWL_LOG_ERROR_F("failed to set AudioChannelLayout (device:%u)", p_device_id);
        }
        return false;
    }

    Error error;
    CoreAudioUtilities::set_input_sample_rate(p_audio_unit, p_properties.audio_format.sample_rate, error);
    if (error.has_error() && !silent) {
        LOWL_LOG_ERROR_F("failed to set input sample rate (device:%u)", p_device_id);
        return false;
    }

    SampleRate output_sample_rate = CoreAudioUtilities::get_output_sample_rate(p_audio_unit, error);
    if (error.has_error() && !silent) {
        LOWL_LOG_ERROR_F("failed to get output sample rate (device:%u)", p_device_id);
        return false;
    }
    if (!Lowl::Audio::sample_rates_equal(output_sample_rate, p_properties.audio_format.sample_rate)) {
        return false;
    }

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
        dispose_audio_unit(new_audio_unit);
        return nullptr;
    }

    result = AudioUnitSetProperty(new_audio_unit,
                                  kAudioOutputUnitProperty_CurrentDevice,
                                  kAudioUnitScope_Global,
                                  CoreAudioUtilities::kOutputBus,
                                  &p_device_id,
                                  sizeof(AudioDeviceID));
    if (result != noErr) {
        LOWL_LOG_ERROR_F("failed to set property: CurrentDevice (device:%u, OSStatus:%u)", p_device_id, result);
        error.set_vendor_error(result, Error::VendorError::CoreAudioVendorError);
        dispose_audio_unit(new_audio_unit);
        return nullptr;
    }

    return new_audio_unit;
}

bool Lowl::Audio::CoreAudioDevice::create_description(const Lowl::Audio::AudioDeviceProperties &p_device_properties,
                                                      AudioStreamBasicDescription &r_description,
                                                      Error &error) {
    const AudioFormat &audio_format = p_device_properties.audio_format;
    if (!audio_format.is_valid()) {
        error.set_error(ErrorCode::InvalidParameter);
        return false;
    }

    AudioStreamBasicDescription description{};
    description.mFormatID = kAudioFormatLinearPCM;
    description.mSampleRate = audio_format.sample_rate;
    description.mFramesPerPacket = 1;
    description.mBitsPerChannel = static_cast<UInt32>(get_sample_size_bits(p_device_properties.sample_format));
    description.mChannelsPerFrame = static_cast<UInt32>(audio_format.channel_layout.channel_count);

    switch (p_device_properties.sample_format) {
        case Lowl::Audio::SampleFormat::FLOAT_32: {
            description.mFormatFlags = kAudioFormatFlagsNativeFloatPacked | kAudioFormatFlagIsNonInterleaved;
            description.mBytesPerPacket = static_cast<UInt32>(get_sample_size_bytes(p_device_properties.sample_format));
            description.mBytesPerFrame = static_cast<UInt32>(get_sample_size_bytes(p_device_properties.sample_format));
            break;
        }
        case Lowl::Audio::SampleFormat::INT_32: {
            description.mFormatFlags =
                kAudioFormatFlagIsSignedInteger | kAudioFormatFlagsNativeEndian | kAudioFormatFlagIsPacked;
            description.mBytesPerPacket = static_cast<UInt32>(get_sample_size_bytes(p_device_properties.sample_format) *
                                                              audio_format.channel_layout.channel_count);
            description.mBytesPerFrame = static_cast<UInt32>(get_sample_size_bytes(p_device_properties.sample_format) *
                                                             audio_format.channel_layout.channel_count);
            break;
        }
        case Lowl::Audio::SampleFormat::INT_24: {
            description.mFormatFlags =
                kAudioFormatFlagIsSignedInteger | kAudioFormatFlagsNativeEndian | kAudioFormatFlagIsPacked;
            description.mBytesPerPacket = static_cast<UInt32>(get_sample_size_bytes(p_device_properties.sample_format) *
                                                              audio_format.channel_layout.channel_count);
            description.mBytesPerFrame = static_cast<UInt32>(get_sample_size_bytes(p_device_properties.sample_format) *
                                                             audio_format.channel_layout.channel_count);
            break;
        }
        case Lowl::Audio::SampleFormat::INT_16: {
            description.mFormatFlags =
                kAudioFormatFlagIsSignedInteger | kAudioFormatFlagsNativeEndian | kAudioFormatFlagIsPacked;
            description.mBytesPerPacket = static_cast<UInt32>(get_sample_size_bytes(p_device_properties.sample_format) *
                                                              audio_format.channel_layout.channel_count);
            description.mBytesPerFrame = static_cast<UInt32>(get_sample_size_bytes(p_device_properties.sample_format) *
                                                             audio_format.channel_layout.channel_count);
            break;
        }
        case Lowl::Audio::SampleFormat::INT_8: {
            description.mFormatFlags =
                kAudioFormatFlagIsSignedInteger | kAudioFormatFlagsNativeEndian | kAudioFormatFlagIsPacked;
            description.mBytesPerPacket = static_cast<UInt32>(get_sample_size_bytes(p_device_properties.sample_format) *
                                                              audio_format.channel_layout.channel_count);
            description.mBytesPerFrame = static_cast<UInt32>(get_sample_size_bytes(p_device_properties.sample_format) *
                                                             audio_format.channel_layout.channel_count);
            break;
        }
        case Lowl::Audio::SampleFormat::U_INT_8: {
            error.set_error(ErrorCode::UnsupportedAudioFormat);
            return false;
        }
        case Lowl::Audio::SampleFormat::FLOAT_64: {
            description.mFormatFlags = kAudioFormatFlagsNativeFloatPacked | kAudioFormatFlagIsNonInterleaved;
            description.mBytesPerPacket = static_cast<UInt32>(get_sample_size_bytes(p_device_properties.sample_format));
            description.mBytesPerFrame = static_cast<UInt32>(get_sample_size_bytes(p_device_properties.sample_format));
            break;
        }
        case Lowl::Audio::SampleFormat::Unknown: {
            error.set_error(ErrorCode::UnsupportedAudioFormat);
            return false;
        }
    }

    r_description = description;
    return true;
}

void Lowl::Audio::CoreAudioDevice::release_hog() {
    Error error;

    if (hog_pid == getpid()) {
        // i hogged
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

void Lowl::Audio::CoreAudioDevice::cleanup_audio_unit(Error &error) {
    unpublish_render_state();

    if (audio_unit_initialized && audio_unit != nullptr) {
        OSStatus result = AudioOutputUnitStop(audio_unit);
        if (result != noErr) {
            set_error_if_clear(error, result);
        }

        result = AudioUnitReset(audio_unit, kAudioUnitScope_Global, 0);
        if (result != noErr) {
            set_error_if_clear(error, result);
        }
    }

    if (running_listener_registered && audio_unit != nullptr) {
        const OSStatus result = AudioUnitRemovePropertyListenerWithUserData(
            audio_unit, kAudioOutputUnitProperty_IsRunning, &osx_start_stop_callback, this);
        if (result != noErr) {
            set_error_if_clear(error, result);
        }
        running_listener_registered = false;
    }

    if (audio_unit_initialized && audio_unit != nullptr) {
        const OSStatus result = AudioUnitUninitialize(audio_unit);
        if (result != noErr) {
            set_error_if_clear(error, result);
        }
        audio_unit_initialized = false;
    }

    if (device_property_listener_registered) {
        Error listener_error;
        CoreAudioUtilities::remove_property_listener(
            device_id,
            kAudioDeviceProcessorOverload,
            kAudioDevicePropertyScopeOutput,
            osx_property_callback,
            this,
            listener_error);
        if (listener_error.has_error()) {
            set_error_if_clear(error, static_cast<OSStatus>(listener_error.get_vendor_error()));
        }
        device_property_listener_registered = false;
    }

    release_hog();
    dispose_audio_unit(audio_unit, &error);
    if (!release_render_state() && !error.has_error()) {
        error.set_error(ErrorCode::InvalidOperationWhileActive);
    }
    audio_source.reset();
}

void Lowl::Audio::CoreAudioDevice::cleanup_failed_start() {
    Error cleanup_error;
    cleanup_audio_unit(cleanup_error);
    if (cleanup_error.has_error()) {
        LOWL_LOG_ERROR_F("CoreAudioDevice::start cleanup failed (device:%u)", device_id);
    }
}

#endif /* LOWL_DRIVER_CORE_AUDIO */
