#ifdef LOWL_DRIVER_WASAPI

#include "lowl_audio_wasapi_device.h"

#include "audio/convert/lowl_audio_sample_converter.h"
#include "lowl_logger.h"

#include <functional>

#define SAFE_CLOSE(h) if ((h) != NULL) { CloseHandle((h)); (h) = NULL; }
#define SAFE_RELEASE(punk) if ((punk) != NULL) { (punk)->Release(); (punk) = NULL; }

// @formatter:off
static const PROPERTYKEY LOWL_PKEY_Device_FriendlyName = {{0xA45C254E, 0xDF1C, 0x4EFD, {0x80, 0x20, 0x67, 0xD1, 0x46, 0xA8, 0x50, 0xE0}}, 14};
static const PROPERTYKEY LOWL_PKEY_AudioEngine_DeviceFormat = {{0xF19F064D, 0x82C, 0x4E27, {0xBC, 0x73, 0x68, 0x82, 0xA1, 0xBB, 0x8E, 0x4C}}, 0};
static const IID LOWL_IID_IAudioClient = {0x1CB9AD4C, 0xDBFA, 0x4C32, {0xB1, 0x78, 0xC2, 0xF5, 0x68, 0xA7, 0x03,0xB2}}; /* 1CB9AD4C-DBFA-4C32-B178-C2F568A703B2 = __uuidof(IAudioClient) */
static const IID LOWL_IID_IAudioClient2 = {0x726778CD, 0xF60A, 0x4EDA, {0x82, 0xDE, 0xE4, 0x76, 0x10, 0xCD, 0x78,0xAA}}; /* 726778CD-F60A-4EDA-82DE-E47610CD78AA = __uuidof(IAudioClient2) */
static const IID LOWL_IID_IAudioClient3 = {0x7ED4EE07, 0x8E67, 0x4CD4, {0x8C, 0x1A, 0x2B, 0x7A, 0x59, 0x87, 0xAD,0x42}}; /* 7ED4EE07-8E67-4CD4-8C1A-2B7A5987AD42 = __uuidof(IAudioClient3) */
static const IID LOWL_IID_IAudioRenderClient = {0xF294ACFC, 0x3146, 0x4483, {0xA7, 0xBF, 0xAD, 0xDC, 0xA7, 0xC2, 0x60,0xE2}}; /* F294ACFC-3146-4483-A7BF-ADDCA7C260E2 = __uuidof(IAudioRenderClient) */
GUID LOWL_GUID_KSDATAFORMAT_SUBTYPE_PCM = {0x00000001, 0x0000, 0x0010, {0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}};
GUID LOWL_GUID_KSDATAFORMAT_SUBTYPE_IEEE_FLOAT = {0x00000003, 0x0000, 0x0010,{0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}};
// @formatter:on


static DWORD WINAPI

wasapi_audio_callback(void *param) {
    Lowl::Audio::WasapiDevice *device = (Lowl::Audio::WasapiDevice *) param;
    return device->audio_callback();
}

Lowl::Audio::WasapiDevice::WasapiDevice() {
    default_sample_rate = 0;
    output_channel = Lowl::Audio::AudioChannel::None;
    sample_format = Lowl::Audio::SampleFormat::Unknown;
    wasapi_device = nullptr;
    audio_client = nullptr;
    wasapi_audio_thread_handle = nullptr;
    wasapi_audio_event_handle = nullptr;
    audio_render_client = nullptr;
    share_mode = AUDCLNT_SHAREMODE_SHARED;
}

Lowl::Audio::WasapiDevice::~WasapiDevice() {
}


void Lowl::Audio::WasapiDevice::populate_device_properties() {

    IAudioClient *tmp_audio_client = nullptr;
    HRESULT result = wasapi_device->Activate(LOWL_IID_IAudioClient3, CLSCTX_ALL, nullptr, (void **) &tmp_audio_client);
    if (FAILED(result)) {
        return;
    }

    WAVEFORMATEXTENSIBLE wfe;

    // test for exclusive mode
    memset(&wfe, 0, sizeof(wfe));
    wfe.Format.cbSize = sizeof(wfe);
    wfe.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
    wfe.Format.nChannels = (WORD) get_channel_num(output_channel);
    wfe.Format.nSamplesPerSec = (DWORD) default_sample_rate;
    wfe.Format.wBitsPerSample = (WORD) Lowl::Audio::get_sample_bits(sample_format);
    wfe.Format.nBlockAlign = (wfe.Format.nChannels * wfe.Format.wBitsPerSample) / 8;
    wfe.Format.nAvgBytesPerSec = wfe.Format.nBlockAlign * wfe.Format.nSamplesPerSec;
    wfe.Samples.wValidBitsPerSample = wfe.Format.wBitsPerSample;
    wfe.dwChannelMask = SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT;
    wfe.SubFormat = get_wave_sub_format(sample_format);
    result = tmp_audio_client->IsFormatSupported(
            AUDCLNT_SHAREMODE_EXCLUSIVE,
            (WAVEFORMATEX * ) & wfe,
            nullptr
    );
    if (result == S_OK) {
        AudioDeviceProperties property = AudioDeviceProperties();
        property.channel = output_channel;
        property.sample_rate = default_sample_rate;
        property.sample_format = sample_format;
        property.exclusive_mode = true;
        properties.push_back(property);
    } else {
        switch (result) {
            case AUDCLNT_E_UNSUPPORTED_FORMAT:
                LOWL_LOG_DEBUG_F("%s -> IsFormatSupported(AUDCLNT_SHAREMODE_EXCLUSIVE): AUDCLNT_E_UNSUPPORTED_FORMAT",
                                 get_name().c_str());
                break;
            case E_POINTER:
                LOWL_LOG_DEBUG_F("%s -> IsFormatSupported(AUDCLNT_SHAREMODE_EXCLUSIVE): E_POINTER", get_name().c_str());
                break;
            case E_INVALIDARG:
                LOWL_LOG_DEBUG_F("%s -> IsFormatSupported(AUDCLNT_SHAREMODE_EXCLUSIVE): E_INVALIDARG",
                                 get_name().c_str());
                break;
            case AUDCLNT_E_DEVICE_INVALIDATED:
                LOWL_LOG_DEBUG_F("%s -> IsFormatSupported(AUDCLNT_SHAREMODE_EXCLUSIVE): AUDCLNT_E_DEVICE_INVALIDATED",
                                 get_name().c_str());
                break;
            case AUDCLNT_E_SERVICE_NOT_RUNNING:
                LOWL_LOG_DEBUG_F("%s -> IsFormatSupported(AUDCLNT_SHAREMODE_EXCLUSIVE): AUDCLNT_E_SERVICE_NOT_RUNNING",
                                 get_name().c_str());
                break;
            default:
                LOWL_LOG_DEBUG_F("%s -> IsFormatSupported(AUDCLNT_SHAREMODE_EXCLUSIVE): Unknown HRESULT(0x%lx)",
                                 get_name().c_str(), result);
                break;
        }
    }


    // test for share modes
    WAVEFORMATEX *closest_match;
    memset(&closest_match, 0, sizeof(closest_match));

    memset(&wfe, 0, sizeof(wfe));
    wfe.Format.cbSize = sizeof(wfe);
    wfe.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
    wfe.Format.nChannels = (WORD) get_channel_num(output_channel);
    wfe.Format.nSamplesPerSec = (DWORD) default_sample_rate;
    wfe.Format.wBitsPerSample = (WORD) Lowl::Audio::get_sample_bits(sample_format);
    wfe.Format.nBlockAlign = (wfe.Format.nChannels * wfe.Format.wBitsPerSample) / 8;
    wfe.Format.nAvgBytesPerSec = wfe.Format.nBlockAlign * wfe.Format.nSamplesPerSec;
    wfe.Samples.wValidBitsPerSample = wfe.Format.wBitsPerSample;
    wfe.dwChannelMask = SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT;
    wfe.SubFormat = get_wave_sub_format(sample_format);

    result = tmp_audio_client->IsFormatSupported(
            AUDCLNT_SHAREMODE_SHARED,
            (WAVEFORMATEX * ) & wfe,
            &closest_match
    );

    if (result == S_OK && closest_match == nullptr) {
        AudioDeviceProperties property = AudioDeviceProperties();
        property.channel = output_channel;
        property.sample_rate = default_sample_rate;
        property.sample_format = sample_format;
        property.exclusive_mode = false;
        properties.push_back(property);
    } else if (result == S_FALSE && closest_match != nullptr) {
        AudioDeviceProperties property = AudioDeviceProperties();
        property.channel = Lowl::Audio::get_channel(closest_match->nChannels);
        property.sample_rate = closest_match->nSamplesPerSec;
        property.sample_format = get_sample_format(closest_match);;
        property.exclusive_mode = false;
        properties.push_back(property);
    } else {
        switch (result) {
            case AUDCLNT_E_UNSUPPORTED_FORMAT:
                LOWL_LOG_DEBUG_F("%s -> IsFormatSupported(AUDCLNT_SHAREMODE_SHARED): AUDCLNT_E_UNSUPPORTED_FORMAT",
                                 get_name().c_str());
                break;
            case E_POINTER:
                LOWL_LOG_DEBUG_F("%s -> IsFormatSupported(AUDCLNT_SHAREMODE_SHARED): E_POINTER", get_name().c_str());
                break;
            case E_INVALIDARG:
                LOWL_LOG_DEBUG_F("%s -> IsFormatSupported(AUDCLNT_SHAREMODE_SHARED): E_INVALIDARG",
                                 get_name().c_str());
                break;
            case AUDCLNT_E_DEVICE_INVALIDATED:
                LOWL_LOG_DEBUG_F("%s -> IsFormatSupported(AUDCLNT_SHAREMODE_SHARED): AUDCLNT_E_DEVICE_INVALIDATED",
                                 get_name().c_str());
                break;
            case AUDCLNT_E_SERVICE_NOT_RUNNING:
                LOWL_LOG_DEBUG_F("%s -> IsFormatSupported(AUDCLNT_SHAREMODE_SHARED): AUDCLNT_E_SERVICE_NOT_RUNNING",
                                 get_name().c_str());
                break;
            default:
                LOWL_LOG_DEBUG_F("%s -> IsFormatSupported(AUDCLNT_SHAREMODE_SHARED): Unknown HRESULT(0x%lx)",
                                 get_name().c_str(), result);
                break;
        }
    }


}


void Lowl::Audio::WasapiDevice::start(std::shared_ptr<AudioSource> p_audio_source, Lowl::Error &error) {
    audio_source = p_audio_source;

    HRESULT result = S_OK;
    if (audio_client == nullptr) {
        result = wasapi_device->Activate(LOWL_IID_IAudioClient3, CLSCTX_ALL, nullptr, (void **) &audio_client);
        if (FAILED(result)) {
            return;
        }
    }

    REFERENCE_TIME default_device_period = 0;
    REFERENCE_TIME minimum_device_period = 0;
    result = audio_client->GetDevicePeriod(&default_device_period, &minimum_device_period);
    if (FAILED(result)) {
        return;
    }

    WAVEFORMATEX *closest_match;
    if (share_mode == AUDCLNT_SHAREMODE_SHARED) {
        memset(&closest_match, 0, sizeof(closest_match));
    } else if (share_mode == AUDCLNT_SHAREMODE_EXCLUSIVE) {
        closest_match = nullptr;
    } else {
        // error
        return;
    }

    WAVEFORMATEXTENSIBLE wfe;
    memset(&wfe, 0, sizeof(wfe));
    wfe.Format.cbSize = sizeof(wfe);
    wfe.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
    wfe.Format.nChannels = (WORD) get_channel_num(output_channel);
    wfe.Format.nSamplesPerSec = (DWORD) default_sample_rate;
    wfe.Format.wBitsPerSample = (WORD) Lowl::Audio::get_sample_bits(sample_format);
    wfe.Format.nBlockAlign = (wfe.Format.nChannels * wfe.Format.wBitsPerSample) / 8;
    wfe.Format.nAvgBytesPerSec = wfe.Format.nBlockAlign * wfe.Format.nSamplesPerSec;
    wfe.Samples.wValidBitsPerSample = wfe.Format.wBitsPerSample;
    wfe.dwChannelMask = SPEAKER_FRONT_LEFT |
                        SPEAKER_FRONT_RIGHT; // https://docs.microsoft.com/en-us/windows-hardware/drivers/ddi/ksmedia/ns-ksmedia-waveformatextensible#remarks
    wfe.SubFormat = get_wave_sub_format(sample_format);

    result = audio_client->IsFormatSupported(
            share_mode,
            (WAVEFORMATEX * ) & wfe,
            &closest_match
    );

    if (share_mode == AUDCLNT_SHAREMODE_SHARED) {
        if (result == S_OK && closest_match == nullptr) {
            // if the audio engine supports the caller-specified format, IsFormatSupported sets *ppClosestMatch to NULL and returns S_OK.
        } else if (result == S_FALSE && closest_match != nullptr) {
            int iasda = 1;
            // if the audio engine supports the caller-specified format, IsFormatSupported sets *ppClosestMatch to NULL and returns S_OK.
        }
    } else if (share_mode == AUDCLNT_SHAREMODE_EXCLUSIVE) {
        if (result == S_OK && closest_match == nullptr) {
            // For exclusive mode, IsFormatSupported returns S_OK if the audio endpoint device supports the caller-specified format, or it returns AUDCLNT_E_UNSUPPORTED_FORMAT if the device does not support the format
        }
    }

    if (FAILED(result)) {
        switch (result) {
            case AUDCLNT_E_UNSUPPORTED_FORMAT:
                break;
            case E_POINTER:
                break;
            case E_INVALIDARG:
                break;
            case AUDCLNT_E_DEVICE_INVALIDATED:
                break;
            case AUDCLNT_E_SERVICE_NOT_RUNNING:
                break;
        }

        return;
    }

    result = audio_client->Initialize(
            share_mode,
            AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
            minimum_device_period,
            minimum_device_period,
            (WAVEFORMATEX * ) & wfe,
            nullptr
    );
    if (FAILED(result)) {
        return;
    }

    result = audio_client->GetService(
            LOWL_IID_IAudioRenderClient,
            (void **) &audio_render_client
    );
    if (FAILED(result)) {
        return;
    }

    wasapi_audio_event_handle = CreateEvent(nullptr, false, false, nullptr);
    if (wasapi_audio_event_handle == INVALID_HANDLE_VALUE) {
        return;
    }

    result = audio_client->SetEventHandle(wasapi_audio_event_handle);
    if (FAILED(result)) {
        return;
    }

    wasapi_audio_thread_handle = CreateThread(
            nullptr,
            0,
            wasapi_audio_callback,
            this,
            0,
            nullptr
    );
    if (wasapi_audio_thread_handle == nullptr) {
        return;
    }
    SetThreadPriority(wasapi_audio_thread_handle, THREAD_PRIORITY_HIGHEST);

    result = audio_client->Start();
    if (FAILED(result)) {
        return;
    }

}

uint32_t Lowl::Audio::WasapiDevice::audio_callback() {

    while (true) {
        DWORD state = WaitForSingleObject(wasapi_audio_event_handle, INFINITE);
        switch (state) {
            case WAIT_ABANDONED:
                break;
            case WAIT_OBJECT_0:
                break;
            case WAIT_TIMEOUT:
                break;
            case WAIT_FAILED:
                break;
        }

        UINT32 frames_available_in_buffer;
        HRESULT result = audio_client->GetBufferSize(&frames_available_in_buffer);
        if (FAILED(result)) {
            return 0;
        }

        if (share_mode == AUDCLNT_SHAREMODE_SHARED) {
            UINT32 padding_frames_count;
            result = audio_client->GetCurrentPadding(&padding_frames_count);
            if (FAILED(result)) {
                return 0;
            }
            frames_available_in_buffer = frames_available_in_buffer - padding_frames_count;
        }


        BYTE *pData;
        result = audio_render_client->GetBuffer(frames_available_in_buffer, &pData);
        if (FAILED(result)) {
            switch (result) {
                case AUDCLNT_E_BUFFER_ERROR:
                    break;
                case AUDCLNT_E_BUFFER_TOO_LARGE: {
                    int asd = 1;
                    break;
                }
                case AUDCLNT_E_BUFFER_SIZE_ERROR:
                    break;
                case AUDCLNT_E_OUT_OF_ORDER:
                    break;
                case AUDCLNT_E_DEVICE_INVALIDATED:
                    break;
                case AUDCLNT_E_BUFFER_OPERATION_PENDING:
                    break;
                case AUDCLNT_E_SERVICE_NOT_RUNNING:
                    break;
                case E_POINTER:
                    break;
            }

            return 0;
        }

        int32_t *dst = (int32_t *) pData;

        unsigned long current_frame = 0;
        AudioFrame frame{};
        for (; current_frame < frames_available_in_buffer; current_frame++) {
            AudioSource::ReadResult read_result = audio_source->read(frame);
            if (read_result == AudioSource::ReadResult::Read) {
                for (int current_channel = 0; current_channel < audio_source->get_channel_num(); current_channel++) {
                    Sample sample = std::clamp(
                            frame[current_channel],
                            AudioFrame::MIN_SAMPLE_VALUE,
                            AudioFrame::MAX_SAMPLE_VALUE
                    );
                    double scaled = sample * 0x7FFFFFFF;
                    int32_t r = (int32_t) scaled;
                    *dst++ = r;
                }
            } else if (read_result == AudioSource::ReadResult::End) {
                break;
            } else if (read_result == AudioSource::ReadResult::Pause) {
                break;
            } else if (read_result == AudioSource::ReadResult::Remove) {
                break;
            }
        }

        DWORD flags = 0;
        if (current_frame < frames_available_in_buffer) {
            flags |= AUDCLNT_BUFFERFLAGS_SILENT;
        }

        result = audio_render_client->ReleaseBuffer(frames_available_in_buffer, flags);
        if (FAILED(result)) {
            return 0;
        }
    }

}

void Lowl::Audio::WasapiDevice::stop(Lowl::Error &error) {
    //  CloseHandle(hThreadArray[i]);
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
    result = device_properties->GetValue(LOWL_PKEY_Device_FriendlyName, &value);
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

    Lowl::SampleRate device_sample_rate = wave_format->nSamplesPerSec;
    Lowl::Audio::AudioChannel device_output_channel = Lowl::Audio::get_channel(wave_format->nChannels);
    Lowl::Audio::SampleFormat device_sample_format = get_sample_format(wave_format);

    PropVariantClear(&value);
    std::unique_ptr<WasapiDevice> device = std::make_unique<WasapiDevice>();
    device->set_name("[" + p_driver_name + "] " + std::string(device_name));
    device->default_sample_rate = device_sample_rate;
    device->output_channel = device_output_channel;
    device->wasapi_device = wasapi_device;
    device->sample_format = device_sample_format;
    device->populate_device_properties();
    return device;
}

char *Lowl::Audio::WasapiDevice::wc_to_utf8(const wchar_t *p_wc) {
    int ulen = WideCharToMultiByte(CP_UTF8, 0, p_wc, -1, nullptr, 0, nullptr, nullptr);
    char *ubuf = new char[ulen + 1];
    WideCharToMultiByte(CP_UTF8, 0, p_wc, -1, ubuf, ulen, nullptr, nullptr);
    ubuf[ulen] = 0;
    return ubuf;
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


Lowl::Audio::SampleFormat Lowl::Audio::WasapiDevice::get_sample_format(const WAVEFORMATEX *p_wave_format_ex) {
    if (p_wave_format_ex->wFormatTag == WAVE_FORMAT_EXTENSIBLE) {
        const WAVEFORMATEXTENSIBLE *pWFEX = (const WAVEFORMATEXTENSIBLE *) p_wave_format_ex;

        if (IsEqualGUID(*(const GUID *) &pWFEX->SubFormat, *(const GUID *) &LOWL_GUID_KSDATAFORMAT_SUBTYPE_PCM)) {
            if (pWFEX->Samples.wValidBitsPerSample == 32) {
                return Lowl::Audio::SampleFormat::INT_32;
            }
            if (pWFEX->Samples.wValidBitsPerSample == 24) {
                if (pWFEX->Format.wBitsPerSample == 24) {
                    return Lowl::Audio::SampleFormat::INT_24;
                }
                if (pWFEX->Format.wBitsPerSample == 32) {
                    return Lowl::Audio::SampleFormat::INT_32;
                }
            }
            if (pWFEX->Samples.wValidBitsPerSample == 16) {
                return Lowl::Audio::SampleFormat::INT_16;
            }
            if (pWFEX->Samples.wValidBitsPerSample == 8) {
                return Lowl::Audio::SampleFormat::U_INT_8;
            }
        }
        if (IsEqualGUID(*(const GUID *) &pWFEX->SubFormat,
                        *(const GUID *) &LOWL_GUID_KSDATAFORMAT_SUBTYPE_IEEE_FLOAT)) {
            if (pWFEX->Samples.wValidBitsPerSample == 32) {
                return Lowl::Audio::SampleFormat::FLOAT_32;
            }
        }
    } else {
        if (p_wave_format_ex->wFormatTag == WAVE_FORMAT_PCM) {
            if (p_wave_format_ex->wBitsPerSample == 32) {
                return Lowl::Audio::SampleFormat::INT_32;
            }
            if (p_wave_format_ex->wBitsPerSample == 24) {
                return Lowl::Audio::SampleFormat::INT_24;
            }
            if (p_wave_format_ex->wBitsPerSample == 16) {
                return Lowl::Audio::SampleFormat::INT_16;
            }
            if (p_wave_format_ex->wBitsPerSample == 8) {
                return Lowl::Audio::SampleFormat::U_INT_8;
            }
        }
        if (p_wave_format_ex->wFormatTag == WAVE_FORMAT_IEEE_FLOAT) {
            if (p_wave_format_ex->wBitsPerSample == 32) {
                return Lowl::Audio::SampleFormat::FLOAT_32;
            }
        }
    }

    return Lowl::Audio::SampleFormat::Unknown;
}

#endif
