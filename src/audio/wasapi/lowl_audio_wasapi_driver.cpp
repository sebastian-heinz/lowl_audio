#ifdef LOWL_DRIVER_WASAPI

#include "lowl_audio_wasapi_driver.h"
#include "lowl_audio_wasapi_device.h"

#include <mmdeviceapi.h>
#include <propidl.h>

#define SAFE_CLOSE(h) if ((h) != NULL) { CloseHandle((h)); (h) = NULL; }
#define SAFE_RELEASE(punk) if ((punk) != NULL) { (punk)->Release(); (punk) = NULL; }


Lowl::Audio::WasapiDriver::WasapiDriver() {
    name = std::string("WASAPI");
}

Lowl::Audio::WasapiDriver::~WasapiDriver() {
}

void Lowl::Audio::WasapiDriver::initialize(Lowl::Error &error) {
    create_devices(error);
}

void Lowl::Audio::WasapiDriver::create_devices(Lowl::Error &error) {

    IMMDeviceEnumerator *enumerator = nullptr;
    HRESULT result = CoCreateInstance(__uuidof(MMDeviceEnumerator),
                                      nullptr,
                                      CLSCTX_INPROC_SERVER,
                                      __uuidof(IMMDeviceEnumerator),
                                      (void **) &enumerator
    );
    if (FAILED(result)) {
        SAFE_RELEASE(enumerator)
        return;
    }

    //IMMDevice *device = nullptr;
    //result = enumerator->GetDefaultAudioEndpoint(
    //        _EDataFlow::eRender,
    //        _ERole::eMultimedia,
    //        &device
    //);

    IMMDeviceCollection *end_points = nullptr;
    result = enumerator->EnumAudioEndpoints(
            eRender,
            DEVICE_STATE_ACTIVE,
            &end_points
    );
    if (FAILED(result)) {
        SAFE_RELEASE(end_points)
        SAFE_RELEASE(enumerator)
        return;
    }

    UINT device_count;
    result = end_points->GetCount(&device_count);
    if (FAILED(result)) {
        SAFE_RELEASE(end_points)
        SAFE_RELEASE(enumerator)
        return;
    }

    for (UINT i = 0; i < device_count; i++) {
        IMMDevice *wasapi_device = nullptr;
        result = end_points->Item(i, &wasapi_device);
        if (FAILED(result)) {
            SAFE_RELEASE(wasapi_device)
            continue;
        }
        Error err;
        std::shared_ptr<Lowl::Audio::AudioDevice> device = WasapiDevice::construct(
                name,
                wasapi_device,
                err
        );
        if (err.ok()) {
            devices.push_back(device);
        } else {
            SAFE_RELEASE(wasapi_device);
        }
    }

    SAFE_RELEASE(end_points)
    SAFE_RELEASE(enumerator)
}

#endif
