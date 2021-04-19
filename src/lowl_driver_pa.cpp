#ifdef LOWL_DRIVER_PORTAUDIO

#include "lowl_driver_pa.h"

#include "lowl_device_pa.h"

void Lowl::PaDriver::create_devices(Error &error) {
    devices.clear();

    PaDeviceIndex default_device_index = Pa_GetDefaultOutputDevice();
    if (default_device_index == paNoDevice) {
        // no default device
        // TODO log warning
    }

    PaHostApiIndex api_count = Pa_GetHostApiCount();
    for (PaHostApiIndex api_index = 0; api_index < api_count; api_index++) {
        const PaHostApiInfo *api_info = Pa_GetHostApiInfo(api_index);
        for (PaDeviceIndex api_device_index = 0; api_device_index < api_info->deviceCount; api_device_index++) {
            PaDeviceIndex device_index = Pa_HostApiDeviceIndexToDeviceIndex(api_index, api_device_index);
            const PaDeviceInfo *device_info = Pa_GetDeviceInfo(device_index);
            if (device_info->maxOutputChannels <= 0) {
                // only output
                continue;
            }
            std::string name = "[" + std::string(api_info->name) + "] " + std::string(device_info->name);
            std::shared_ptr<PaDevice> device = std::make_shared<PaDevice>();
            device->set_name(name);
            device->set_device_index(device_index);
            devices.push_back(device);

            if (default_device) {
                // already set
                // TODO log warning
                continue;
            }
            if (device_index == default_device_index) {
                default_device = device;
            }
        }
    }
}

void Lowl::PaDriver::initialize(Error &error) {
    create_devices(error);
}

Lowl::PaDriver::PaDriver() : Driver() {
    name = std::string("Port Audio");
}

Lowl::PaDriver::~PaDriver() {
}

#endif