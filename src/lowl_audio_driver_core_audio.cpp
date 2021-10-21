#ifdef LOWL_DRIVER_CORE_AUDIO

#include "lowl_audio_driver_core_audio.h"


#include "lowl_logger.h"

//#include <AudioUnit/AudioUnit.h>
//#include <AudioToolbox/AudioToolbox.h>
//#include <CoreAudio/CoreAudio.h>
#include <CoreAudio/AudioHardware.h>

void Lowl::AudioDriverCoreAudio::initialize(Lowl::Error &error) {
    create_devices(error);
}

void Lowl::AudioDriverCoreAudio::create_devices(Error &error) {
    devices.clear();


    UInt32 outPropertySize;
    AudioObjectPropertyAddress device_property = {
            kAudioHardwarePropertyDevices,
            kAudioObjectPropertyScopeGlobal,
            kAudioObjectPropertyElementMaster
    };
    OSStatus s = AudioObjectGetPropertyDataSize(kAudioObjectSystemObject,
                                                &device_property,
                                                0,
                                                nullptr,
                                                &outPropertySize
    );
    // allocate
    uint32_t device_count = outPropertySize / sizeof(AudioObjectID);
    std::vector<AudioObjectID> device_ids = std::vector<AudioObjectID>(device_count);

    OSStatus sa1 = AudioObjectGetPropertyData(kAudioObjectSystemObject,
                                              &device_property,
                                              0,
                                              nullptr,
                                              &outPropertySize,
                                              device_ids.data()
    );

    AudioObjectPropertyAddress default_device_property = {
            kAudioHardwarePropertyDefaultOutputDevice,
            kAudioObjectPropertyScopeGlobal,
            kAudioObjectPropertyElementMaster
    };
    AudioObjectID default_out_device_id = kAudioObjectUnknown;
    UInt32 audio_object_size = sizeof(AudioObjectID);
    OSStatus sa2 = AudioObjectGetPropertyData(kAudioObjectSystemObject,
                                              &default_device_property,
                                              0,
                                              nullptr,
                                              &audio_object_size,
                                              &default_out_device_id
    );

    for (AudioObjectID device_id : device_ids) {
        Error device_error;
        std::shared_ptr<AudioDeviceCoreAudio> device = create_device(device_id, device_error);
        if (device_error.has_error()) {
            continue;
        }

        devices.push_back(device);
        if (device_id == default_out_device_id) {
            if (default_device) {
                Logger::log(Logger::Level::Warn,
                            "Lowl::AudioDriverCoreAudio::create_devices: default_device already assigned");
                continue;
            }
            default_device = device;
            Logger::log(Logger::Level::Debug,
                        "Lowl::AudioDriverCoreAudio::create_devices: default_device assigned: " + name);
        }
    }
}

std::shared_ptr<Lowl::AudioDeviceCoreAudio>
Lowl::AudioDriverCoreAudio::create_device(AudioObjectID device_id, Lowl::Error &error) {

    // AudioObjectPropertyAddress input_streams_property = {
    //         kAudioDevicePropertyStreams,
    //         kAudioDevicePropertyScopeInput,
    //         kAudioObjectPropertyElementMaster
    // };
    // UInt32 dataSize = 0;
    // OSStatus status = AudioObjectGetPropertyDataSize(device_id,
    //                                                  &input_streams_property,
    //                                                  0,
    //                                                  nullptr,
    //                                                  &dataSize);
    // UInt32 streamCount = dataSize / sizeof(AudioStreamID);
    // if (streamCount > 0) {
    //     // has input
    // }

    AudioObjectPropertyAddress output_streams_property = {
            kAudioDevicePropertyStreams,
            kAudioDevicePropertyScopeOutput,
            kAudioObjectPropertyElementMaster
    };

    UInt32 dataSize = 0;
    OSStatus status = AudioObjectGetPropertyDataSize(device_id,
                                                     &output_streams_property,
                                                     0,
                                                     nullptr,
                                                     &dataSize);
    UInt32 streamCount = dataSize / sizeof(AudioStreamID);

    if (streamCount <= 0) {
        // has output
        error.set_error(ErrorCode::Error);
        return std::shared_ptr<AudioDeviceCoreAudio>();
    }


    // AudioObjectPropertyScope scope = isInput ? kAudioDevicePropertyScopeInput : kAudioDevicePropertyScopeOutput;
    // AudioObjectPropertyScope scope = isInput ? kAudioDevicePropertyScopeInput : kAudioDevicePropertyScopeOutput;
    CFStringRef nameRef;
    UInt32 propSize = sizeof(nameRef);
    AudioObjectPropertyAddress name_property = {
            kAudioObjectPropertyName,
            kAudioDevicePropertyScopeOutput,
            0
    };
    OSStatus status1 = AudioObjectGetPropertyData(
            device_id,
            &name_property,
            0,
            nullptr,
            &propSize,
            &nameRef
    );

    long device_name_str_size = CFStringGetMaximumSizeForEncoding(CFStringGetLength(nameRef), kCFStringEncodingUTF8);
    char *device_name_str = new char[(unsigned long) device_name_str_size + 1];
    CFStringGetCString(nameRef, device_name_str, device_name_str_size + 1, kCFStringEncodingUTF8);
    CFRelease(nameRef);

    std::string device_name = std::string(device_name_str);
    delete[] device_name_str;


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


#endif