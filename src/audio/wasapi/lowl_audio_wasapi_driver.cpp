#ifdef LOWL_DRIVER_WASAPI

#include "lowl_audio_wasapi_driver.h"
#include "lowl_audio_wasapi_device.h"

#include <mmdeviceapi.h>
#include <propidl.h>
//#include <functiondiscoverykeys.h>
//#include <setupapi.h>
//#include <initguid.h>
//#include <devpkey.h>
#include <functiondiscoverykeys_devpkey.h>

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
            _EDataFlow::eRender,
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
        std::shared_ptr <Lowl::Audio::AudioDevice> device = create_device(wasapi_device);
        SAFE_RELEASE(wasapi_device)
        devices.push_back(device);
    }

    SAFE_RELEASE(end_points)
    SAFE_RELEASE(enumerator)
}

std::unique_ptr <Lowl::Audio::AudioDevice> Lowl::Audio::WasapiDriver::create_device(void *p_wasapi_device) {

    IMMDevice *wasapi_device = (IMMDevice *) p_wasapi_device;

    WCHAR *wasapi_device_id;
    HRESULT result = wasapi_device->GetId(&wasapi_device_id);
    if (FAILED(result)) {
        return nullptr;
    }
    size_t device_id_len = wcslen(wasapi_device_id);
    WCHAR *device_id = new WCHAR[device_id_len + 1];
    wcsncpy(device_id, wasapi_device_id, device_id_len);
    device_id[device_id_len + 1] = '\0';
    CoTaskMemFree(wasapi_device_id);

    DWORD device_state = 0;
    result = wasapi_device->GetState(&device_state);
    if (FAILED(result)) {
        return nullptr;
    }
    if (device_state != DEVICE_STATE_ACTIVE) {
        return nullptr;
    }

    IPropertyStore *device_properties;
    result = wasapi_device->OpenPropertyStore(STGM_READ, &device_properties);
    if (FAILED(result)) {
        SAFE_RELEASE(device_properties)
        return nullptr;
    }

    PROPVARIANT value;
    PropVariantInit(&value);
    device_properties->GetValue(PKEY_Device_FriendlyName, &value);
    if (FAILED(result)) {
        PropVariantClear(&value);
        SAFE_RELEASE(device_properties)
        return nullptr;
    }
    // WideCharToMultiByte(CP_UTF8, 0, value.pwszVal, -1, pInfo->name, sizeof(pInfo->name), 0, FALSE);
    // WideCharToMultiByte(CP_UTF8, 0, value.pwszVal, (INT32) wcslen(value.pwszVal), (char *) deviceInfo->name,
    //                      PA_WASAPI_DEVICE_NAME_LEN - 1, 0, 0);
    PropVariantClear(&value);


    PropVariantInit(&value);
    device_properties->GetValue(PKEY_AudioEngine_DeviceFormat, &value);
    if (FAILED(result)) {
        PropVariantClear(&value);
        SAFE_RELEASE(device_properties)
        return nullptr;
    }
    // memcpy(&wasapiDeviceInfo->DefaultFormat, value.blob.pBlobData,
    //        min(sizeof(wasapiDeviceInfo->DefaultFormat), value.blob.cbSize));
    PropVariantClear(&value);

    std::unique_ptr <WasapiDevice> device = std::make_unique<WasapiDevice>();
    // TODO set / init device
    return device;
}

#endif