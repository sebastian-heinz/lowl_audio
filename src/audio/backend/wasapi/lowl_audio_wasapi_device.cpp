#ifdef LOWL_DRIVER_WASAPI

#include "lowl_audio_wasapi_device.h"

#include <avrt.h>
#include <objbase.h>

#include <algorithm>
#include <functional>
#include <thread>

#include "audio/convert/lowl_audio_sample_converter.h"
#include "audio/lowl_audio_setting.h"
#include "lowl_logger.h"

#define SAFE_CLOSE(h)                                                                                                  \
    if ((h) != nullptr) {                                                                                              \
        CloseHandle((h));                                                                                              \
        (h) = nullptr;                                                                                                 \
    }
#define SAFE_RELEASE(punk)                                                                                             \
    if ((punk) != nullptr) {                                                                                           \
        (punk)->Release();                                                                                             \
        (punk) = nullptr;                                                                                              \
    }

// @formatter:off
static const PROPERTYKEY LOWL_PKEY_Device_FriendlyName = {
    {0xA45C254E, 0xDF1C, 0x4EFD, {0x80, 0x20, 0x67, 0xD1, 0x46, 0xA8, 0x50, 0xE0}}, 14};
static const PROPERTYKEY LOWL_PKEY_AudioEngine_DeviceFormat = {
    {0xF19F064D, 0x82C, 0x4E27, {0xBC, 0x73, 0x68, 0x82, 0xA1, 0xBB, 0x8E, 0x4C}}, 0};
static const IID LOWL_IID_IAudioClient = {0x1CB9AD4C,
                                          0xDBFA,
                                          0x4C32,
                                          {0xB1,
                                           0x78,
                                           0xC2,
                                           0xF5,
                                           0x68,
                                           0xA7,
                                           0x03,
                                           0xB2}}; /* 1CB9AD4C-DBFA-4C32-B178-C2F568A703B2 = __uuidof(IAudioClient) */
static const IID LOWL_IID_IAudioClient2 = {0x726778CD,
                                           0xF60A,
                                           0x4EDA,
                                           {0x82,
                                            0xDE,
                                            0xE4,
                                            0x76,
                                            0x10,
                                            0xCD,
                                            0x78,
                                            0xAA}}; /* 726778CD-F60A-4EDA-82DE-E47610CD78AA = __uuidof(IAudioClient2) */
static const IID LOWL_IID_IAudioClient3 = {0x7ED4EE07,
                                           0x8E67,
                                           0x4CD4,
                                           {0x8C,
                                            0x1A,
                                            0x2B,
                                            0x7A,
                                            0x59,
                                            0x87,
                                            0xAD,
                                            0x42}}; /* 7ED4EE07-8E67-4CD4-8C1A-2B7A5987AD42 = __uuidof(IAudioClient3) */
static const IID LOWL_IID_IAudioRenderClient = {
    0xF294ACFC,
    0x3146,
    0x4483,
    {0xA7,
     0xBF,
     0xAD,
     0xDC,
     0xA7,
     0xC2,
     0x60,
     0xE2}}; /* F294ACFC-3146-4483-A7BF-ADDCA7C260E2 = __uuidof(IAudioRenderClient) */
GUID LOWL_GUID_KSDATAFORMAT_SUBTYPE_PCM = {
    0x00000001, 0x0000, 0x0010, {0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}};
GUID LOWL_GUID_KSDATAFORMAT_SUBTYPE_IEEE_FLOAT = {
    0x00000003, 0x0000, 0x0010, {0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}};

static DWORD WINAPI

wasapi_audio_callback(void *param) {
    Lowl::Audio::WasapiDevice *device = (Lowl::Audio::WasapiDevice *)param;
    return device->audio_callback();
}
// @formatter:on

Lowl::Audio::WasapiDevice::WasapiDevice(_constructor_tag ct) : AudioDevice(ct) {
    wasapi_device = nullptr;
    audio_client = nullptr;
    wasapi_audio_thread_handle = nullptr;
    wasapi_audio_event_handle = nullptr;
    wasapi_audio_stop_handle = nullptr;
    audio_render_client = nullptr;
    avrt_handle = nullptr;
    avrt_task_index = 0;
}

void Lowl::Audio::WasapiDevice::start(AudioDeviceProperties p_audio_device_properties,
                                      std::shared_ptr<AudioSource> p_audio_source,
                                      Lowl::Error &error) {
    LOWL_LOG_DEBUG_F("start->%s", name.c_str());

    Lowl::Error stop_error;
    stop(stop_error);

    HRESULT result = S_OK;

    if (!p_audio_device_properties.is_supported) {
        error.set_error(Lowl::ErrorCode::Error);
        return;
    }
    if (p_audio_source != nullptr &&
        p_audio_source->get_audio_format() != p_audio_device_properties.get_audio_format()) {
        LOWL_LOG_ERROR("WasapiDevice::start: source AudioFormat does not match the device AudioFormat.");
        error.set_error(Lowl::ErrorCode::InvalidParameter);
        return;
    }

    audio_device_properties = p_audio_device_properties;
    audio_source = p_audio_source;

    result = wasapi_device->Activate(LOWL_IID_IAudioClient3, CLSCTX_ALL, nullptr, (void **)&audio_client);
    if (result != S_OK) {
        LOWL_LOG_DEBUG_F("start->%s - audio_client->Activate:FAILED", name.c_str());
        error.set_error(Lowl::ErrorCode::Error);
        cleanup_failed_start();
        return;
    }
    LOWL_LOG_DEBUG_F("start->%s - audio_client->Activate:OK", name.c_str());

    REFERENCE_TIME default_device_period = 0;
    REFERENCE_TIME minimum_device_period = 0;
    result = audio_client->GetDevicePeriod(&default_device_period, &minimum_device_period);
    if (result != S_OK) {
        LOWL_LOG_DEBUG_F("start->%s - audio_client->GetDevicePeriod:FAILED", name.c_str());
        error.set_error(Lowl::ErrorCode::Error);
        cleanup_failed_start();
        return;
    }
    LOWL_LOG_DEBUG_F("start->%s - audio_client->GetDevicePeriod:OK", name.c_str());

    WAVEFORMATEXTENSIBLE wfe = to_wave_format_extensible(audio_device_properties);
    AUDCLNT_SHAREMODE share_mode =
        audio_device_properties.exclusive_mode ? AUDCLNT_SHAREMODE_EXCLUSIVE : AUDCLNT_SHAREMODE_SHARED;

    REFERENCE_TIME periodicity = minimum_device_period;
    if (share_mode == AUDCLNT_SHAREMODE_SHARED) {
        periodicity = 0;
    }

    result = audio_client->Initialize(share_mode,
                                      AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
                                      minimum_device_period,
                                      periodicity,
                                      (WAVEFORMATEX *)&wfe,
                                      nullptr);

    if (result == AUDCLNT_E_BUFFER_SIZE_NOT_ALIGNED) {
        LOWL_LOG_DEBUG_F("start->%s - Initialize:AUDCLNT_E_BUFFER_SIZE_NOT_ALIGNED - fixing... (%s)",
                         name.c_str(),
                         audio_device_properties.to_string().c_str());

        // Call IAudioClient::GetBufferSize and receive the next-highest-aligned buffer size (in frames).
        UINT32 frames_available_in_buffer;
        result = audio_client->GetBufferSize(&frames_available_in_buffer);
        if (result != S_OK) {
            LOWL_LOG_DEBUG_F("start->%s - Initialize:AUDCLNT_E_BUFFER_SIZE_NOT_ALIGNED - GetBufferSize:FAILED (%s)",
                             name.c_str(),
                             audio_device_properties.to_string().c_str());
            error.set_error(Lowl::ErrorCode::Error);
            cleanup_failed_start();
            return;
        }

        // Call IAudioClient::Release to release the audio client used in the previous call that returned AUDCLNT_E_BUFFER_SIZE_NOT_ALIGNED.
        SAFE_RELEASE(audio_client);

        // Calculate the aligned buffer size in 100-nanosecond units (hns).
        // The buffer size is (REFERENCE_TIME)((10000.0 * 1000 / WAVEFORMATEX.nSamplesPerSecond * nFrames) + 0.5).
        // In this formula, nFrames is the buffer size retrieved by GetBufferSize.
        minimum_device_period =
            (REFERENCE_TIME)(10000.0 * 1000 / wfe.Format.nSamplesPerSec * frames_available_in_buffer) + 0.5;

        // Call the IMMDevice::Activate method with parameter iid set to REFIID IID_IAudioClient to create a new audio client.
        result = wasapi_device->Activate(LOWL_IID_IAudioClient3, CLSCTX_ALL, nullptr, (void **)&audio_client);
        if (result != S_OK) {
            LOWL_LOG_DEBUG_F("start->%s - Initialize:AUDCLNT_E_BUFFER_SIZE_NOT_ALIGNED - Activate:FAILED (%s)",
                             name.c_str(),
                             audio_device_properties.to_string().c_str());
            error.set_error(Lowl::ErrorCode::Error);
            cleanup_failed_start();
            return;
        }

        periodicity = minimum_device_period;
        if (share_mode == AUDCLNT_SHAREMODE_SHARED) {
            periodicity = 0;
        }

        // Call Initialize again on the created audio client and specify the new buffer size and periodicity.
        result = audio_client->Initialize(share_mode,
                                          AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
                                          minimum_device_period,
                                          periodicity,
                                          (WAVEFORMATEX *)&wfe,
                                          nullptr);
        if (result != S_OK) {
            LOWL_LOG_DEBUG_F("start->%s - Initialize:AUDCLNT_E_BUFFER_SIZE_NOT_ALIGNED - Initialize:FAILED (%s)",
                             name.c_str(),
                             audio_device_properties.to_string().c_str());
            error.set_error(Lowl::ErrorCode::Error);
            cleanup_failed_start();
            return;
        }

        LOWL_LOG_DEBUG_F("start->%s - Initialize:AUDCLNT_E_BUFFER_SIZE_NOT_ALIGNED - fixed! (%s)",
                         name.c_str(),
                         audio_device_properties.to_string().c_str());
    } else if (result != S_OK) {
        switch (result) {
            case AUDCLNT_E_ALREADY_INITIALIZED:
                LOWL_LOG_DEBUG_F("start->%s - Initialize:AUDCLNT_E_ALREADY_INITIALIZED (%s)",
                                 name.c_str(),
                                 audio_device_properties.to_string().c_str());
                break;
            case AUDCLNT_E_BUFFER_SIZE_NOT_ALIGNED: {
                LOWL_LOG_DEBUG_F("start->%s - Initialize:AUDCLNT_E_BUFFER_SIZE_NOT_ALIGNED (%s)",
                                 name.c_str(),
                                 audio_device_properties.to_string().c_str());
                break;
            }
            case AUDCLNT_E_BUFFER_SIZE_ERROR:
                LOWL_LOG_DEBUG_F("start->%s - Initialize:AUDCLNT_E_BUFFER_SIZE_ERROR (%s)",
                                 name.c_str(),
                                 audio_device_properties.to_string().c_str());
                break;
            case AUDCLNT_E_CPUUSAGE_EXCEEDED:
                LOWL_LOG_DEBUG_F("start->%s - Initialize:AUDCLNT_E_CPUUSAGE_EXCEEDED (%s)",
                                 name.c_str(),
                                 audio_device_properties.to_string().c_str());
                break;
            case AUDCLNT_E_DEVICE_INVALIDATED:
                LOWL_LOG_DEBUG_F("start->%s - Initialize:AUDCLNT_E_DEVICE_INVALIDATED (%s)",
                                 name.c_str(),
                                 audio_device_properties.to_string().c_str());
                break;
            case AUDCLNT_E_DEVICE_IN_USE:
                LOWL_LOG_DEBUG_F("start->%s - Initialize:AUDCLNT_E_DEVICE_IN_USE (%s)",
                                 name.c_str(),
                                 audio_device_properties.to_string().c_str());
                break;
            case AUDCLNT_E_ENDPOINT_CREATE_FAILED:
                LOWL_LOG_DEBUG_F("start->%s - Initialize:AUDCLNT_E_ENDPOINT_CREATE_FAILED (%s)",
                                 name.c_str(),
                                 audio_device_properties.to_string().c_str());
                break;
            case AUDCLNT_E_INVALID_DEVICE_PERIOD:
                LOWL_LOG_DEBUG_F("start->%s - Initialize:AUDCLNT_E_INVALID_DEVICE_PERIOD (%s)",
                                 name.c_str(),
                                 audio_device_properties.to_string().c_str());
                break;
            case AUDCLNT_E_UNSUPPORTED_FORMAT:
                LOWL_LOG_DEBUG_F("start->%s - Initialize:AUDCLNT_E_UNSUPPORTED_FORMAT (%s)",
                                 name.c_str(),
                                 audio_device_properties.to_string().c_str());
                break;
            case AUDCLNT_E_EXCLUSIVE_MODE_NOT_ALLOWED:
                LOWL_LOG_DEBUG_F("start->%s - Initialize:AUDCLNT_E_EXCLUSIVE_MODE_NOT_ALLOWED (%s)",
                                 name.c_str(),
                                 audio_device_properties.to_string().c_str());
                break;
            case AUDCLNT_E_BUFDURATION_PERIOD_NOT_EQUAL:
                LOWL_LOG_DEBUG_F("start->%s - Initialize:AUDCLNT_E_BUFDURATION_PERIOD_NOT_EQUAL (%s)",
                                 name.c_str(),
                                 audio_device_properties.to_string().c_str());
                break;
            case AUDCLNT_E_SERVICE_NOT_RUNNING:
                LOWL_LOG_DEBUG_F("start->%s - Initialize:AUDCLNT_E_SERVICE_NOT_RUNNING (%s)",
                                 name.c_str(),
                                 audio_device_properties.to_string().c_str());
                break;
            case E_POINTER:
                LOWL_LOG_DEBUG_F(
                    "start->%s - Initialize:E_POINTER (%s)", name.c_str(), audio_device_properties.to_string().c_str());
                break;
            case E_INVALIDARG:
                LOWL_LOG_DEBUG_F("start->%s - Initialize:E_INVALIDARG (%s)",
                                 name.c_str(),
                                 audio_device_properties.to_string().c_str());
                break;
            case E_OUTOFMEMORY:
                LOWL_LOG_DEBUG_F("start->%s - Initialize:E_OUTOFMEMORY (%s)",
                                 name.c_str(),
                                 audio_device_properties.to_string().c_str());
                break;
            default:
                LOWL_LOG_DEBUG_F("start->%s - Initialize:UNKNOWN(%ld) (%s)",
                                 name.c_str(),
                                 result,
                                 audio_device_properties.to_string().c_str());
                break;
        }
        LOWL_LOG_DEBUG_F("start->%s - audio_client->Initialize:FAILED", name.c_str());
        error.set_error(Lowl::ErrorCode::Error);
        cleanup_failed_start();
        return;
    }
    LOWL_LOG_DEBUG_F("start->%s - audio_client->Initialize:OK", name.c_str());

    result = audio_client->GetService(LOWL_IID_IAudioRenderClient, (void **)&audio_render_client);
    if (result != S_OK) {
        LOWL_LOG_DEBUG_F("start->%s - audio_client->GetService:FAILED", name.c_str());
        error.set_error(Lowl::ErrorCode::Error);
        cleanup_failed_start();
        return;
    }
    LOWL_LOG_DEBUG_F("start->%s - audio_client->GetService:OK", name.c_str());

    UINT32 total_frames_in_buffer = 0;
    result = audio_client->GetBufferSize(&total_frames_in_buffer);
    if (result != S_OK) {
        LOWL_LOG_DEBUG_F("start->%s - audio_client->GetBufferSize:FAILED", name.c_str());
        error.set_error(Lowl::ErrorCode::Error);
        cleanup_failed_start();
        return;
    }
    if (!allocate_render_buffer(total_frames_in_buffer)) {
        error.set_error(ErrorCode::InvalidOperationWhileActive);
        cleanup_failed_start();
        return;
    }

    wasapi_audio_stop_handle = CreateEvent(nullptr, false, false, nullptr);
    if (wasapi_audio_stop_handle == INVALID_HANDLE_VALUE || wasapi_audio_stop_handle == nullptr) {
        wasapi_audio_stop_handle = nullptr;
        error.set_error(Lowl::ErrorCode::Error);
        cleanup_failed_start();
        return;
    }

    wasapi_audio_event_handle = CreateEvent(nullptr, false, false, nullptr);
    if (wasapi_audio_event_handle == INVALID_HANDLE_VALUE || wasapi_audio_event_handle == nullptr) {
        wasapi_audio_event_handle = nullptr;
        error.set_error(Lowl::ErrorCode::Error);
        cleanup_failed_start();
        return;
    }

    result = audio_client->SetEventHandle(wasapi_audio_event_handle);
    if (result != S_OK) {
        LOWL_LOG_DEBUG_F("start->%s - audio_client->SetEventHandle:FAILED", name.c_str());
        error.set_error(Lowl::ErrorCode::Error);
        cleanup_failed_start();
        return;
    }
    LOWL_LOG_DEBUG_F("start->%s - audio_client->SetEventHandle:OK", name.c_str());

    wasapi_audio_thread_handle = CreateThread(nullptr, 0, wasapi_audio_callback, this, 0, nullptr);
    if (wasapi_audio_thread_handle == nullptr) {
        error.set_error(Lowl::ErrorCode::Error);
        cleanup_failed_start();
        return;
    }

    result = audio_client->Start();
    if (result != S_OK) {
        LOWL_LOG_DEBUG_F("start->%s - audio_client->Start:FAILED", name.c_str());
        error.set_error(Lowl::ErrorCode::Error);
        cleanup_failed_start();
        return;
    }
    LOWL_LOG_DEBUG_F("start->%s - audio_client->Start:OK", name.c_str());

    LOWL_LOG_DEBUG_F("started->%s", name.c_str());
}

void Lowl::Audio::WasapiDevice::cleanup_failed_start() {
    Lowl::Error cleanup_error;
    stop(cleanup_error);
    if (cleanup_error.has_error()) {
        LOWL_LOG_ERROR_F("WasapiDevice::start cleanup failed (%s)", name.c_str());
    }
}

void Lowl::Audio::WasapiDevice::stop(Lowl::Error &error) {
    LOWL_LOG_DEBUG_F("stop->%s", name.c_str());
    unpublish_render_state();

    if (audio_client != nullptr) {
        HRESULT result = audio_client->Stop();
        if (result != S_OK) {
            LOWL_LOG_DEBUG_F("stop->%s - audio_client->Stop:FAILED (%ld)", name.c_str(), result);
        }
    }

    if (wasapi_audio_thread_handle) {
        SetEvent(wasapi_audio_stop_handle);
        WaitForSingleObject(wasapi_audio_thread_handle, INFINITE);
    }

    SAFE_RELEASE(audio_client)
    SAFE_RELEASE(audio_render_client)
    SAFE_CLOSE(wasapi_audio_stop_handle)
    SAFE_CLOSE(wasapi_audio_event_handle)
    SAFE_CLOSE(wasapi_audio_thread_handle)
    if (!release_render_state() && !error.has_error()) {
        error.set_error(ErrorCode::InvalidOperationWhileActive);
    }
    audio_source.reset();
    LOWL_LOG_DEBUG_F("stopped->%s", name.c_str());
}

bool Lowl::Audio::WasapiDevice::enable_avrt() {
    if (avrt_handle != nullptr) {
        AvRevertMmThreadCharacteristics(avrt_handle);
        LOWL_LOG_DEBUG_F("enable_avrt->%s - AvRevertMmThreadCharacteristics (avrt_handle != nullptr)", name.c_str());
    }
    avrt_task_index = 0;
    avrt_handle = AvSetMmThreadCharacteristics("Pro Audio", &avrt_task_index);
    if (avrt_handle == nullptr) {
        LOWL_LOG_DEBUG_F("enable_avrt->%s - AvSetMmThreadCharacteristics failed", name.c_str());
        return false;
    }
    if (!AvSetMmThreadPriority(avrt_handle, AVRT_PRIORITY::AVRT_PRIORITY_CRITICAL)) {
        LOWL_LOG_DEBUG_F("enable_avrt->%s - AvSetMmThreadPriority failed", name.c_str());
        return false;
    }
    return true;
}

uint32_t Lowl::Audio::WasapiDevice::audio_callback() {
    const HRESULT com_result = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    const bool should_uninitialize_com = SUCCEEDED(com_result);
    if (FAILED(com_result) && com_result != RPC_E_CHANGED_MODE) {
        return 0;
    }

    UINT32 total_frames_in_buffer;
    HRESULT result = audio_client->GetBufferSize(&total_frames_in_buffer);
    if (FAILED(result)) {
        if (should_uninitialize_com) {
            CoUninitialize();
        }
        return 0;
    }
    UINT32 available_frames_in_buffer = total_frames_in_buffer;

    if (!enable_avrt()) {
        SetThreadPriority(wasapi_audio_thread_handle, THREAD_PRIORITY_HIGHEST);
    }

    HANDLE wait_array[] = {wasapi_audio_stop_handle, wasapi_audio_event_handle};
    bool playing = true;
    while (playing) {
        DWORD wait_result = WaitForMultipleObjects(std::size(wait_array), wait_array, false, INFINITE);

        switch (wait_result) {
            case WAIT_OBJECT_0 + 0: // wasapi_audio_stop_handle
                playing = false;
                continue;
            case WAIT_OBJECT_0 + 1: // wasapi_audio_event_handle
                break;
            default:
                break;
        }

        if (!audio_device_properties.exclusive_mode) {
            // share_mode == AUDCLNT_SHAREMODE_SHARED
            UINT32 padding_frames_count;
            result = audio_client->GetCurrentPadding(&padding_frames_count);
            if (FAILED(result)) {
                break;
            }
            if (total_frames_in_buffer <= padding_frames_count) {
                available_frames_in_buffer = 0;
            } else {
                available_frames_in_buffer = total_frames_in_buffer - padding_frames_count;
            }
        }

        BYTE *audio_buffer_byte_ptr = nullptr;
        result = audio_render_client->GetBuffer(available_frames_in_buffer, &audio_buffer_byte_ptr);
        if (FAILED(result)) {
            break;
        }

        RenderState *const published_state = load_render_state();
        if (published_state == nullptr) {
            result = audio_render_client->ReleaseBuffer(available_frames_in_buffer, AUDCLNT_BUFFERFLAGS_SILENT);
            if (FAILED(result)) {
                break;
            }
            continue;
        }

        const AudioDeviceProperties &published_properties = published_state->audio_device_properties;
        const uint32_t bytes_per_frame =
            static_cast<uint32_t>(get_sample_size_bytes(published_properties.sample_format) *
                                  published_properties.audio_format.channel_layout.channel_count);
        render_to_device_buffer(published_state,
                                audio_buffer_byte_ptr,
                                static_cast<size_t>(available_frames_in_buffer) * bytes_per_frame,
                                available_frames_in_buffer,
                                bytes_per_frame);

        result = audio_render_client->ReleaseBuffer(available_frames_in_buffer, 0);
        if (FAILED(result)) {
            break;
        }
    }

    if (avrt_handle != nullptr) {
        AvRevertMmThreadCharacteristics(avrt_handle);
    }

    if (should_uninitialize_com) {
        CoUninitialize();
    }
    return 0;
}

std::unique_ptr<Lowl::Audio::WasapiDevice>
Lowl::Audio::WasapiDevice::construct(const std::string &p_driver_name, void *p_wasapi_device, Lowl::Error &error) {
    IMMDevice *wasapi_device = (IMMDevice *)p_wasapi_device;
    HRESULT result = S_OK;
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
    result = device_properties->GetValue(LOWL_PKEY_Device_FriendlyName, &value);
    if (FAILED(result)) {
        PropVariantClear(&value);
        SAFE_RELEASE(device_properties)
        return nullptr;
    }
    std::string device_name = "[" + p_driver_name + "] " + WasapiDevice::wc_to_utf8(value.pwszVal);
    PropVariantClear(&value);

    PropVariantInit(&value);
    result = device_properties->GetValue(LOWL_PKEY_AudioEngine_DeviceFormat, &value);
    if (FAILED(result)) {
        PropVariantClear(&value);
        SAFE_RELEASE(device_properties)
        return nullptr;
    }
    WAVEFORMATEX *wave_format = (WAVEFORMATEX *)value.blob.pBlobData;
    if (wave_format == nullptr) {
        PropVariantClear(&value);
        SAFE_RELEASE(device_properties)
        return nullptr;
    }
    std::vector<AudioDeviceProperties> audio_device_properties =
        create_device_properties(wasapi_device, wave_format, device_name);
    PropVariantClear(&value);

    std::unique_ptr<WasapiDevice> device = std::make_unique<WasapiDevice>(_constructor_tag{});
    device->name = device_name;
    device->wasapi_device = wasapi_device;
    device->properties_list = audio_device_properties;

    return device;
}

std::string Lowl::Audio::WasapiDevice::wc_to_utf8(const wchar_t *p_wc) {
    if (p_wc == nullptr) {
        return {};
    }
    int ulen = WideCharToMultiByte(CP_UTF8, 0, p_wc, -1, nullptr, 0, nullptr, nullptr);
    if (ulen <= 0) {
        return {};
    }
    std::vector<char> ubuf(static_cast<size_t>(ulen), '\0');
    WideCharToMultiByte(CP_UTF8, 0, p_wc, -1, ubuf.data(), ulen, nullptr, nullptr);
    return std::string(ubuf.data());
}

GUID Lowl::Audio::WasapiDevice::get_wave_sub_format(const Lowl::Audio::SampleFormat p_sample_format) {
    switch (p_sample_format) {
        case Lowl::Audio::SampleFormat::U_INT_8:
        case Lowl::Audio::SampleFormat::INT_16:
        case Lowl::Audio::SampleFormat::INT_24:
        case Lowl::Audio::SampleFormat::INT_32: {
            return LOWL_GUID_KSDATAFORMAT_SUBTYPE_PCM;
        }
        case Lowl::Audio::SampleFormat::FLOAT_32: {
            return LOWL_GUID_KSDATAFORMAT_SUBTYPE_IEEE_FLOAT;
        }
        default:
            return GUID_NULL;
    }
}

/***
 * https://www.ambisonic.net/mulchaud.html#_Toc446153097
 * To keep the WAVEFORMATEXTENSIBLE structure under the 64-bit limit, wValidBitsPerSample and wSamplesPerBlock were joined together in a union called Samples.
 * An extra field named wReserved was also added for future use.
 *
 * Details about wValidBitsPerSample
 * The field wValidBitsPerSample is used to explicitly indicate how many bits of precision are present in the signal.
 * Most of the time this value will be equal to wBitsPerSample.
 * If, however, wave data originated from a 20-bit A/D, then wValidBitsPerSample could be set to 20, even though wBitsPerSample might be 24 or 32.
 * Examples are included in sections 4.6 and later.
 * If wValidBitsPerSample is less than wBitsPerSample, then the actual PCM data is "left-aligned" within the container.
 * The sample itself is justified most significant; all extra bits are at the least-significant portion of the container.
 * The value of wValidBitsPerSample should never exceed that of wBitsPerSample.
 * If this is encountered, the proper action is to reject the data format.
 * An entity can change wValidBitsPerSample as it processes the data.
 * For example, an application would know that a stream with wValidBitsPerSample = 24 must be dithered to 16 bits if the output driver indicated that it supported wValidBitsPerSample = 16 only.
 * Although this can be very expensive from a memory-bandwidth standpoint, wBitsPerSample can be changed as well.
 * wValidBitsPerSample indicates whether the container size (wBitsPerSample) can be reduced without data loss.
 * A stream with wValidBitsPerSample = 20; wBitsPerSample = 32 (for processing by a 32-bit CPU) could safely be compressed to wBitsPerSample = 24 (for archiving to disk).
 * Without wValidBitsPerSample, one would not know whether this was lossless.
 *
 * Details about wSamplesPerBlock
 * It is often times useful to know how many samples are contained in one compressed block of audio data.
 * The wSamplesPerBlock is used in compressed formats that have a fixed number of samples within each block.
 * This value aids in buffer estimation and position information.
 * If wSamplesPerBlock is 0, a variable amount of samples are contained in each block of compressed audio data.
 * In this case, buffer estimation and position information need to be obtained in other ways.
 *
 * Details about wReserved
 * If neither wValidBitsPerSample or wSamplesPerBlock apply to the audio data being described by the WAVEFORMATEXTENSIBLE structure, set the wReserved field to 0.
 *
 * @param p_wave_format_ex
 * @return
 */
Lowl::Audio::AudioDeviceProperties
Lowl::Audio::WasapiDevice::to_audio_device_properties(const WAVEFORMATEX *p_wave_format_ex) {
    AudioDeviceProperties properties = AudioDeviceProperties();
    properties.audio_format =
        AudioFormat{static_cast<SampleRate>(p_wave_format_ex->nSamplesPerSec),
                    ChannelLayout::from_count(static_cast<uint8_t>(p_wave_format_ex->nChannels))};
    properties.sample_format = Lowl::Audio::SampleFormat::Unknown;

    if (p_wave_format_ex->wFormatTag == WAVE_FORMAT_EXTENSIBLE) {
        const WAVEFORMATEXTENSIBLE *wave_format_extensible = (const WAVEFORMATEXTENSIBLE *)p_wave_format_ex;

        properties.wasapi.valid_bits_per_sample = wave_format_extensible->Samples.wValidBitsPerSample;
        properties.audio_format.channel_layout =
            wave_format_extensible->dwChannelMask != 0
                ? ChannelLayout::from_mask(wave_format_extensible->dwChannelMask)
                : ChannelLayout::from_count(static_cast<uint8_t>(p_wave_format_ex->nChannels));

        if (IsEqualGUID(*(const GUID *)&wave_format_extensible->SubFormat,
                        *(const GUID *)&LOWL_GUID_KSDATAFORMAT_SUBTYPE_PCM)) {
            if (wave_format_extensible->Samples.wValidBitsPerSample == 32) {
                properties.sample_format = Lowl::Audio::SampleFormat::INT_32;
            }
            if (wave_format_extensible->Samples.wValidBitsPerSample == 24) {
                if (wave_format_extensible->Format.wBitsPerSample == 24) {
                    properties.sample_format = Lowl::Audio::SampleFormat::INT_24;
                }
                if (wave_format_extensible->Format.wBitsPerSample == 32) {
                    properties.sample_format = Lowl::Audio::SampleFormat::INT_32;
                }
            }
            if (wave_format_extensible->Samples.wValidBitsPerSample == 16) {
                properties.sample_format = Lowl::Audio::SampleFormat::INT_16;
            }
            if (wave_format_extensible->Samples.wValidBitsPerSample == 8) {
                properties.sample_format = Lowl::Audio::SampleFormat::U_INT_8;
            }
        }
        if (IsEqualGUID(*(const GUID *)&wave_format_extensible->SubFormat,
                        *(const GUID *)&LOWL_GUID_KSDATAFORMAT_SUBTYPE_IEEE_FLOAT)) {
            if (wave_format_extensible->Samples.wValidBitsPerSample == 32) {
                properties.sample_format = Lowl::Audio::SampleFormat::FLOAT_32;
            }
        }
    } else {
        if (p_wave_format_ex->wFormatTag == WAVE_FORMAT_PCM) {
            if (p_wave_format_ex->wBitsPerSample == 32) {
                properties.sample_format = Lowl::Audio::SampleFormat::INT_32;
            }
            if (p_wave_format_ex->wBitsPerSample == 24) {
                properties.sample_format = Lowl::Audio::SampleFormat::INT_24;
            }
            if (p_wave_format_ex->wBitsPerSample == 16) {
                properties.sample_format = Lowl::Audio::SampleFormat::INT_16;
            }
            if (p_wave_format_ex->wBitsPerSample == 8) {
                properties.sample_format = Lowl::Audio::SampleFormat::U_INT_8;
            }
        }
        if (p_wave_format_ex->wFormatTag == WAVE_FORMAT_IEEE_FLOAT) {
            if (p_wave_format_ex->wBitsPerSample == 32) {
                properties.sample_format = Lowl::Audio::SampleFormat::FLOAT_32;
            }
        }
    }

    return properties;
}

WAVEFORMATEXTENSIBLE Lowl::Audio::WasapiDevice::to_wave_format_extensible(
    const Lowl::Audio::AudioDeviceProperties &audio_device_properties) {
    WAVEFORMATEXTENSIBLE wfe = WAVEFORMATEXTENSIBLE();
    wfe.Format.cbSize = sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX);
    wfe.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
    wfe.Format.nChannels = (WORD)audio_device_properties.audio_format.channel_layout.channel_count;
    wfe.Format.nSamplesPerSec = (DWORD)audio_device_properties.audio_format.sample_rate;

    if (audio_device_properties.wasapi.valid_bits_per_sample > 0) {
        wfe.Format.wBitsPerSample = (WORD)Lowl::Audio::get_sample_bits(audio_device_properties.sample_format);
        wfe.Samples.wValidBitsPerSample = audio_device_properties.wasapi.valid_bits_per_sample;
    } else {
        wfe.Format.wBitsPerSample = (WORD)Lowl::Audio::get_sample_bits(audio_device_properties.sample_format);
        wfe.Samples.wValidBitsPerSample = (WORD)Lowl::Audio::get_sample_bits(audio_device_properties.sample_format);
    }

    wfe.Format.nBlockAlign = (wfe.Format.nChannels * wfe.Format.wBitsPerSample) / 8;
    wfe.Format.nAvgBytesPerSec = wfe.Format.nBlockAlign * wfe.Format.nSamplesPerSec;

    wfe.dwChannelMask = audio_device_properties.audio_format.channel_layout.speaker_mask;
    wfe.SubFormat = get_wave_sub_format(audio_device_properties.sample_format);
    return wfe;
}

std::vector<Lowl::Audio::AudioDeviceProperties> Lowl::Audio::WasapiDevice::create_device_properties(
    IMMDevice *p_wasapi_device, const WAVEFORMATEX *p_wave_format, std::string device_name) {
    std::vector<Lowl::Audio::AudioDeviceProperties> properties_list = std::vector<Lowl::Audio::AudioDeviceProperties>();

    Error error;

    // device default properties
    AudioDeviceProperties default_properties = to_audio_device_properties(p_wave_format);
    std::vector<Lowl::Audio::AudioDeviceProperties> default_properties_list =
        create_device_properties(p_wasapi_device, default_properties, device_name, error);
    properties_list.insert(properties_list.end(), default_properties_list.begin(), default_properties_list.end());

    // test other capabilities
    std::vector<double> test_sample_rates = Lowl::Audio::AudioSetting::get_test_sample_rates();
    std::vector<SampleFormat> test_sample_formats = Lowl::Audio::AudioSetting::get_test_sample_formats();
    std::vector<ChannelLayout> test_channel_layouts = Lowl::Audio::AudioSetting::get_test_channel_layouts();
    for (int sample_format_index = 0; sample_format_index < test_sample_formats.size(); sample_format_index++) {
        for (int sample_rate_index = 0; sample_rate_index < test_sample_rates.size(); sample_rate_index++) {
            for (const ChannelLayout &probe_layout : test_channel_layouts) {
                AudioDeviceProperties test_properties = AudioDeviceProperties();
                test_properties.sample_format = test_sample_formats[sample_format_index];
                test_properties.audio_format = AudioFormat{test_sample_rates[sample_rate_index], probe_layout};

                std::vector<Lowl::Audio::AudioDeviceProperties> test_properties_list =
                    create_device_properties(p_wasapi_device, test_properties, device_name, error);
                properties_list.insert(properties_list.end(), test_properties_list.begin(), test_properties_list.end());
            }
        }
    }

    std::sort(properties_list.begin(), properties_list.end());
    properties_list.erase(std::unique(properties_list.begin(), properties_list.end()), properties_list.end());

    return properties_list;
}

Lowl::Audio::AudioDeviceProperties
Lowl::Audio::WasapiDevice::validate(IMMDevice *p_wasapi_device, const AudioDeviceProperties p_device_properties) {
    AudioDeviceProperties ret = p_device_properties;
    ret.is_supported = false;

    IAudioClient *tmp_audio_client = nullptr;
    HRESULT result = p_wasapi_device->Activate(LOWL_IID_IAudioClient3, CLSCTX_ALL, nullptr, (void **)&tmp_audio_client);
    if (result != S_OK) {
        SAFE_RELEASE(tmp_audio_client);
        return ret;
    }

    WAVEFORMATEXTENSIBLE wfe = to_wave_format_extensible(p_device_properties);
    AUDCLNT_SHAREMODE share_mode = AUDCLNT_SHAREMODE_EXCLUSIVE;
    WAVEFORMATEX *closest_match = nullptr;
    if (!p_device_properties.exclusive_mode) {
        memset(&closest_match, 0, sizeof(closest_match));
        share_mode = AUDCLNT_SHAREMODE_SHARED;
    }

    result = tmp_audio_client->IsFormatSupported(share_mode, (WAVEFORMATEX *)&wfe, &closest_match);

    if (p_device_properties.exclusive_mode) {
        // no failure allowed
        if (result != S_OK) {
            if (closest_match != nullptr) {
                CoTaskMemFree(closest_match);
            }
            SAFE_RELEASE(tmp_audio_client);
            return ret;
        }
    } else {
        // not exclusive check for closest match
        if (result == S_OK && closest_match == nullptr) {
            // properties worked as is
        } else if (result == S_FALSE && closest_match != nullptr) {
            // properties did not work, but we have the closest match.
            ret = to_audio_device_properties(closest_match);
            wfe = to_wave_format_extensible(ret);
        } else {
            switch (result) {
                case AUDCLNT_E_UNSUPPORTED_FORMAT:
                    LOWL_LOG_DEBUG_F("IsFormatSupported: AUDCLNT_E_UNSUPPORTED_FORMAT %s", ret.to_string().c_str());
                    break;
                case E_POINTER:
                    LOWL_LOG_DEBUG_F("IsFormatSupported: E_POINTER %s", ret.to_string().c_str());
                    break;
                case E_INVALIDARG:
                    LOWL_LOG_DEBUG_F("IsFormatSupported: E_INVALIDARG %s", ret.to_string().c_str());
                    break;
                case AUDCLNT_E_DEVICE_INVALIDATED:
                    LOWL_LOG_DEBUG_F("IsFormatSupported: AUDCLNT_E_DEVICE_INVALIDATED %s", ret.to_string().c_str());
                    break;
                case AUDCLNT_E_SERVICE_NOT_RUNNING:
                    LOWL_LOG_DEBUG_F("IsFormatSupported: AUDCLNT_E_SERVICE_NOT_RUNNING %s", ret.to_string().c_str());
                    break;
                default:
                    LOWL_LOG_DEBUG_F("IsFormatSupported: UNKNOWN(%ld) %s", result, ret.to_string().c_str());
                    break;
            }

            if (closest_match != nullptr) {
                CoTaskMemFree(closest_match);
            }
            SAFE_RELEASE(tmp_audio_client);
            return ret;
        }
    }

    if (closest_match != nullptr) {
        CoTaskMemFree(closest_match);
    }
    SAFE_RELEASE(tmp_audio_client);
    ret.is_supported = true;
    return ret;
}

std::vector<Lowl::Audio::AudioDeviceProperties>
Lowl::Audio::WasapiDevice::create_device_properties(IMMDevice *p_wasapi_device,
                                                    const AudioDeviceProperties p_device_properties,
                                                    std::string device_name,
                                                    Error &error) {
    std::vector<AudioDeviceProperties> properties = std::vector<Lowl::Audio::AudioDeviceProperties>();

    AudioDeviceProperties exclusive_properties = p_device_properties;
    exclusive_properties.exclusive_mode = true;
    exclusive_properties = validate(p_wasapi_device, exclusive_properties);
    if (exclusive_properties.is_supported) {
        properties.push_back(exclusive_properties);
    }

    AudioDeviceProperties shared_properties = p_device_properties;
    shared_properties.exclusive_mode = false;
    shared_properties = validate(p_wasapi_device, shared_properties);
    if (shared_properties.is_supported) {
        properties.push_back(shared_properties);
    }

    return properties;
}

Lowl::Audio::WasapiDevice::~WasapiDevice() {
    Lowl::Error error;
    stop(error);
    SAFE_RELEASE(audio_client)
    SAFE_RELEASE(wasapi_device)
    SAFE_RELEASE(audio_render_client)
    SAFE_CLOSE(wasapi_audio_event_handle)
    SAFE_CLOSE(wasapi_audio_thread_handle)
    SAFE_CLOSE(wasapi_audio_stop_handle)
}

#endif
