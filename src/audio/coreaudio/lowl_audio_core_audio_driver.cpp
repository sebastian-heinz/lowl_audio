#ifdef LOWL_DRIVER_CORE_AUDIO

#include "lowl_audio_core_audio_driver.h"

#include "lowl_logger.h"

#include "audio/coreaudio/lowl_audio_core_audio_utilities.h"

void Lowl::Audio::CoreAudioDriver::initialize(Lowl::Error &error) {
    create_devices(error);
}

void Lowl::Audio::CoreAudioDriver::create_devices(Error &error) {
    devices.clear();

    std::vector<AudioObjectID> device_ids = Lowl::Audio::CoreAudioUtilities::get_device_ids();
    AudioObjectID default_out_device_id = Lowl::Audio::CoreAudioUtilities::get_default_device_id();

    for (AudioObjectID device_id : device_ids) {
        Error device_error;
        std::shared_ptr<Lowl::Audio::CoreAudioDevice> device = create_device(device_id, device_error);
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

std::shared_ptr<Lowl::Audio::CoreAudioDevice>
Lowl::Audio::CoreAudioDriver::create_device(AudioObjectID device_id, Lowl::Error &error) {

    uint32_t input_stream_count = Lowl::Audio::CoreAudioUtilities::get_num_stream(device_id,
                                                                                  kAudioDevicePropertyScopeInput);
    uint32_t output_stream_count = Lowl::Audio::CoreAudioUtilities::get_num_stream(device_id,
                                                                                   kAudioDevicePropertyScopeOutput);

    if (output_stream_count <= 0) {
        // has no output
        error.set_error(ErrorCode::Error);
        return std::shared_ptr<Lowl::Audio::CoreAudioDevice>();
    }

    std::string device_name = Lowl::Audio::CoreAudioUtilities::get_device_name(device_id);
    Lowl::SampleRate default_sample_rate = Lowl::Audio::CoreAudioUtilities::get_device_default_sample_rate(device_id);

    uint32_t input_channel_count = Lowl::Audio::CoreAudioUtilities::get_num_channel(
            device_id,
            kAudioDevicePropertyScopeInput
    );
    uint32_t output_channel_count = Lowl::Audio::CoreAudioUtilities::get_num_channel(
            device_id,
            kAudioDevicePropertyScopeOutput
    );
    std::vector <AudioObjectID> streams = Lowl::Audio::CoreAudioUtilities::get_stream_ids(
            device_id,
            kAudioDevicePropertyScopeOutput
    );
    uint32_t latency = Lowl::Audio::CoreAudioUtilities::get_latency(
            device_id,
            streams[0],
            kAudioDevicePropertyScopeOutput
    );

    std::shared_ptr <Lowl::Audio::CoreAudioDevice> device = std::make_shared<Lowl::Audio::CoreAudioDevice>();
    device->set_name("[" + name + "] " + device_name);
    device->set_device_id(device_id);

    return device;
}


Lowl::Audio::CoreAudioDriver::CoreAudioDriver() : AudioDriver() {
    name = std::string("Core Audio");
}

Lowl::Audio::CoreAudioDriver::~CoreAudioDriver() {

}

#endif