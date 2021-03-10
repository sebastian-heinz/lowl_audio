#ifdef LOWL_DRIVER_PORTAUDIO

#include "lowl_pa_driver.h"

#include "lowl_pa_device.h"

void Lowl::PaDriver::create_devices(Error &error) {
    devices.clear();
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
            PaDevice *device = new PaDevice();
            device->set_name(name);
            device->set_device_index(device_index);
            devices.push_back(device);
        }
    }
}

void Lowl::PaDriver::initialize(Error &error) {
    create_devices(error);
}

Lowl::PaDriver::PaDriver() {
    name = std::string("Port Audio");
}

Lowl::PaDriver::~PaDriver() {
}

#endif