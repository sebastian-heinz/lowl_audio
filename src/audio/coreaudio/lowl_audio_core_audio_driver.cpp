#ifdef LOWL_DRIVER_CORE_AUDIO

#include "lowl_audio_core_audio_driver.h"

#include "lowl_logger.h"

#include "audio/coreaudio/lowl_audio_core_audio_utilities.h"

void Lowl::Audio::CoreAudioDriver::initialize(Lowl::Error &error) {
    create_devices(error);
}

void Lowl::Audio::CoreAudioDriver::create_devices(Error &error) {
    devices.clear();

    std::vector<AudioObjectID> device_ids = Lowl::Audio::CoreAudioUtilities::get_device_ids(error);
    AudioObjectID default_out_device_id = Lowl::Audio::CoreAudioUtilities::get_default_device_id(error);

    for (AudioObjectID device_id : device_ids) {
        std::shared_ptr<Lowl::Audio::CoreAudioDevice> device = Lowl::Audio::CoreAudioDevice::create(
                name,
                device_id,
                error
        );
        if (error.has_error()) {
            LOWL_LOG_L_ERROR_F(error, "Device %d - error", device_id);
            error.clear();
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

Lowl::Audio::CoreAudioDriver::CoreAudioDriver() : AudioDriver() {
    name = std::string("Core Audio");
}

Lowl::Audio::CoreAudioDriver::~CoreAudioDriver() {

}

#endif