#ifdef LOWL_DRIVER_WASAPI

#include "lowl_audio_wasapi_device.h"

#include <mmdeviceapi.h>
#include <propidl.h>

//#include <functiondiscoverykeys.h>
//#include <setupapi.h>
#include <initguid.h>
//#include <devpkey.h>

#include <functiondiscoverykeys_devpkey.h>
#include <audioclient.h>

#define SAFE_CLOSE(h) if ((h) != NULL) { CloseHandle((h)); (h) = NULL; }
#define SAFE_RELEASE(punk) if ((punk) != NULL) { (punk)->Release(); (punk) = NULL; }

static const IID LOWL_IID_IAudioClient = { 0x1CB9AD4C, 0xDBFA, 0x4C32, { 0xB1, 0x78, 0xC2, 0xF5, 0x68, 0xA7, 0x03, 0xB2 } }; /* 1CB9AD4C-DBFA-4C32-B178-C2F568A703B2 = __uuidof(IAudioClient) */
static const IID LOWL_IID_IAudioClient2 = { 0x726778CD, 0xF60A, 0x4EDA, { 0x82, 0xDE, 0xE4, 0x76, 0x10, 0xCD, 0x78, 0xAA } }; /* 726778CD-F60A-4EDA-82DE-E47610CD78AA = __uuidof(IAudioClient2) */
static const IID LOWL_IID_IAudioClient3 = { 0x7ED4EE07, 0x8E67, 0x4CD4, { 0x8C, 0x1A, 0x2B, 0x7A, 0x59, 0x87, 0xAD, 0x42 } }; /* 7ED4EE07-8E67-4CD4-8C1A-2B7A5987AD42 = __uuidof(IAudioClient3) */
static const IID LOWL_IID_IAudioRenderClient = { 0xF294ACFC, 0x3146, 0x4483, { 0xA7, 0xBF, 0xAD, 0xDC, 0xA7, 0xC2, 0x60, 0xE2 } }; /* F294ACFC-3146-4483-A7BF-ADDCA7C260E2 = __uuidof(IAudioRenderClient) */


static DWORD WINAPI wasapi_audio_callback(void *param) {
    Lowl::Audio::WasapiDevice *device = (Lowl::Audio::WasapiDevice *) param;
    return device->audio_callback();
}

Lowl::Audio::WasapiDevice::WasapiDevice() {
    default_sample_rate = 44200;
    output_channel = Lowl::Audio::AudioChannel::Stereo;
    sample_format = Lowl::Audio::SampleFormat::FLOAT_32;
    wasapi_device = nullptr;
}

Lowl::Audio::WasapiDevice::~WasapiDevice() {
}

void Lowl::Audio::WasapiDevice::start(std::shared_ptr<AudioSource> p_audio_source, Lowl::Error &error) {

    IAudioClient *audio_client = nullptr;
    IMMDevice *device = (IMMDevice *) wasapi_device;

    HRESULT result = device->Activate(LOWL_IID_IAudioClient3, CLSCTX_ALL, nullptr, (void **)&audio_client);
    if (FAILED(result)) {
        return;
    }

    REFERENCE_TIME DefaultDevicePeriod = 0, MinimumDevicePeriod = 0;
    result = audio_client->GetDevicePeriod(&DefaultDevicePeriod, &MinimumDevicePeriod);
    if (FAILED(result)) {
        return;
    }


    WAVEFORMATEX wave_format = {};
    wave_format.wFormatTag = WAVE_FORMAT_PCM;
    wave_format.nChannels = 2;
    wave_format.nSamplesPerSec = 44100;
    wave_format.nAvgBytesPerSec = 44100 * 2 * 16 / 8;
    wave_format.nBlockAlign = 2 * 16 / 8;
    wave_format.wBitsPerSample = 16;

    result = audio_client->IsFormatSupported(
            AUDCLNT_SHAREMODE_EXCLUSIVE,
            &wave_format,
            nullptr
    );
    if (FAILED(result)) {
        return;
    }

    result = audio_client->Initialize(
            AUDCLNT_SHAREMODE_EXCLUSIVE,
            AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
            MinimumDevicePeriod,
            MinimumDevicePeriod,
            &wave_format,
            nullptr
    );
    if (FAILED(result)) {
        return;
    }

    UINT32 bufferFrameCount;
    result = audio_client->GetBufferSize(&bufferFrameCount);
    if (FAILED(result)) {
        return;
    }

    INT32 FrameSize_bytes = bufferFrameCount * wave_format.nChannels * wave_format.wBitsPerSample / 8;

    IAudioRenderClient *audio_render;
    result = audio_client->GetService(
			LOWL_IID_IAudioRenderClient,
            (void **) &audio_render
    );
    if (FAILED(result)) {
        return;
    }

    HANDLE hEvent = CreateEvent(nullptr, false, false, nullptr);
    if (hEvent == INVALID_HANDLE_VALUE) {
        return;
    }

    result = audio_client->SetEventHandle(hEvent);
    if (FAILED(result)) {
        return;
    }

    HANDLE wasapi_audio_thread_handle = CreateThread(
            nullptr,
            0,
            wasapi_audio_callback,
            this,
            0,
            nullptr
    );

    SetThreadPriority(wasapi_audio_thread_handle, THREAD_PRIORITY_HIGHEST);

    result = audio_client->Start();
    if (FAILED(result)) {
        return;
    }

}

uint32_t Lowl::Audio::WasapiDevice::audio_callback() {



    return 0;
}

void Lowl::Audio::WasapiDevice::stop(Lowl::Error &error) {
  //  CloseHandle(hThreadArray[i]);
}

bool Lowl::Audio::WasapiDevice::is_supported(Lowl::Audio::AudioChannel p_channel, Lowl::SampleRate p_sample_rate,
                                             Lowl::Audio::SampleFormat p_sample_format, Lowl::Error &error) {
    return false;
}

Lowl::SampleRate Lowl::Audio::WasapiDevice::get_default_sample_rate() {
    return default_sample_rate;
}

void Lowl::Audio::WasapiDevice::set_exclusive_mode(bool p_exclusive_mode, Lowl::Error &error) {

}

std::unique_ptr<Lowl::Audio::WasapiDevice>
Lowl::Audio::WasapiDevice::construct(const std::string &p_driver_name, void *p_wasapi_device, Lowl::Error &error) {
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
    result = device_properties->GetValue(PKEY_Device_FriendlyName, &value);
    if (FAILED(result)) {
        PropVariantClear(&value);
        SAFE_RELEASE(device_properties)
        return nullptr;
    }
    const char *device_name = WasapiDevice::wc_to_utf8(value.pwszVal);
    PropVariantClear(&value);


    PropVariantInit(&value);
    result = device_properties->GetValue(PKEY_AudioEngine_DeviceFormat, &value);
    if (FAILED(result)) {
        PropVariantClear(&value);
        SAFE_RELEASE(device_properties)
        return nullptr;
    }

    WAVEFORMATEX *wave_format = (WAVEFORMATEX *) value.blob.pBlobData;
    if (wave_format == nullptr) {
        PropVariantClear(&value);
        SAFE_RELEASE(device_properties)
        return nullptr;
    }

    Lowl::SampleRate default_sample_rate = wave_format->nSamplesPerSec;
    Lowl::Audio::AudioChannel output_channel = Lowl::Audio::get_channel(wave_format->nChannels);
    //sample_format = ma_format_from_WAVEFORMATEX(pWF);

    PropVariantClear(&value);
    std::unique_ptr<WasapiDevice> device = std::make_unique<WasapiDevice>();
    device->set_name("[" + p_driver_name + "] " + std::string(device_name));
    device->default_sample_rate = default_sample_rate;
    device->output_channel = output_channel;
    device->wasapi_device = wasapi_device;
    return device;
}

char *Lowl::Audio::WasapiDevice::wc_to_utf8(const wchar_t *p_wc) {
    int ulen = WideCharToMultiByte(CP_UTF8, 0, p_wc, -1, nullptr, 0, nullptr, nullptr);
    char *ubuf = new char[ulen + 1];
    WideCharToMultiByte(CP_UTF8, 0, p_wc, -1, ubuf, ulen, nullptr, nullptr);
    ubuf[ulen] = 0;
    return ubuf;
}

#endif
