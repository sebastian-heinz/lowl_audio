#ifdef LOWL_DRIVER_CORE_AUDIO

#include "lowl_audio_driver_core_audio.h"

#include "lowl_logger.h"

#include <CoreAudio/AudioHardware.h>

void Lowl::AudioDriverCoreAudio::initialize(Lowl::Error &error) {
    create_devices(error);

   // Lowl::Logger::log(Lowl::Logger::Level::Info, " test");
    LOWL_LOG_INFO_F("test %d", 1);
    LOWL_LOG_INFO("test");
}

void Lowl::AudioDriverCoreAudio::create_devices(Error &error) {
    devices.clear();

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

    AudioObjectPropertyAddress default_device_property = {
            kAudioHardwarePropertyDefaultOutputDevice,
            kAudioObjectPropertyScopeGlobal,
            kAudioObjectPropertyElementMaster
    };
    AudioObjectID default_out_device_id = kAudioObjectUnknown;

    result = AudioObjectGetPropertyData(kAudioObjectSystemObject,
                                        &default_device_property,
                                        0,
                                        nullptr,
                                        &audio_object_size,
                                        &default_out_device_id
    );
    if (result != kAudioHardwareNoError) {
        // err
    }

    for (AudioObjectID device_id : device_ids) {
        Error device_error;
        std::shared_ptr<AudioDeviceCoreAudio> device = create_device(device_id, device_error);
        if (device_error.has_error()) {
            continue;
        }
        devices.push_back(device);
        if (device_id == default_out_device_id) {
            if (default_device) {
                LOWL_LOG_WARN("default_device already assigned");
                continue;
            }
            default_device = device;
            LOWL_LOG_DEBUG("default_device assigned: " + name);
        }
    }
}

std::shared_ptr<Lowl::AudioDeviceCoreAudio>
Lowl::AudioDriverCoreAudio::create_device(AudioObjectID device_id, Lowl::Error &error) {

    uint32_t input_stream_count = get_device_stream_count(device_id, kAudioDevicePropertyScopeInput);
    uint32_t output_stream_count = get_device_stream_count(device_id, kAudioDevicePropertyScopeOutput);

    if (output_stream_count <= 0) {
        // has no output
        error.set_error(ErrorCode::Error);
        return std::shared_ptr<AudioDeviceCoreAudio>();
    }

    std::string device_name = get_device_name(device_id);
    Lowl::SampleRate default_sample_rate = get_device_default_sample_rate(device_id);


//  /* Get the maximum number of input and output channels.  Fail if we can't get this. */

//  err = GetChannelInfo(auhalHostApi, deviceInfo, macCoreDeviceId, 1);
//  if (err)
//      return err;

//  err = GetChannelInfo(auhalHostApi, deviceInfo, macCoreDeviceId, 0);
//  if (err)
//      return err;

    std::shared_ptr<AudioDeviceCoreAudio> device = std::make_shared<AudioDeviceCoreAudio>();
    device->set_name("[" + name + "] " + device_name);
    device->set_device_id(device_id);

    return device;
}

Lowl::AudioDriverCoreAudio::AudioDriverCoreAudio() : AudioDriver() {
    name = std::string("Core Audio");
}

Lowl::AudioDriverCoreAudio::~AudioDriverCoreAudio() {

}

std::string Lowl::AudioDriverCoreAudio::get_device_name(AudioObjectID p_device_id) {
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
Lowl::AudioDriverCoreAudio::get_device_stream_count(AudioObjectID p_device_id, AudioObjectPropertyScope p_scope) {
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

Lowl::SampleRate Lowl::AudioDriverCoreAudio::get_device_default_sample_rate(AudioObjectID p_device_id) {
    uint32_t default_sample_rate_size = sizeof(Float64);
    Float64 default_sample_rate;
    AudioObjectPropertyAddress default_sample_rate_property = {
            kAudioDevicePropertyNominalSampleRate,
            kAudioDevicePropertyScopeOutput,
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


#endif