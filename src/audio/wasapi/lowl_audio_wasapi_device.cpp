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


static DWORD WINAPI wasapi_audio_callback(void *param) {
    Lowl::Audio::WasapiDevice *device = (Lowl::Audio::WasapiDevice *) param;
    return device->audio_callback();
}
// @formatter:on

Lowl::Audio::WasapiDevice::WasapiDevice(_constructor_tag ct) : AudioDevice(ct) {
    wasapi_device = nullptr;
    audio_client = nullptr;
    wasapi_audio_thread_handle = nullptr;
    wasapi_audio_event_handle = nullptr;
    audio_render_client = nullptr;
    audio_device_properties = AudioDeviceProperties();
}

void Lowl::Audio::WasapiDevice::start(AudioDeviceProperties p_audio_device_properties,
                                      std::shared_ptr<AudioSource> p_audio_source,
                                      Lowl::Error &error) {
    audio_device_properties = p_audio_device_properties;
    audio_source = p_audio_source;

    HRESULT result = S_OK;
    if (audio_client == nullptr) {
        result = wasapi_device->Activate(
                LOWL_IID_IAudioClient3,
                CLSCTX_ALL,
                nullptr,
                (void **) &audio_client
        );
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

    AUDCLNT_SHAREMODE share_mode = audio_device_properties.exclusive_mode ? AUDCLNT_SHAREMODE_EXCLUSIVE
                                                                          : AUDCLNT_SHAREMODE_SHARED;

    WAVEFORMATEX *closest_match;
    if (share_mode == AUDCLNT_SHAREMODE_SHARED) {
        memset(&closest_match, 0, sizeof(closest_match));
    } else if (share_mode == AUDCLNT_SHAREMODE_EXCLUSIVE) {
        closest_match = nullptr;
    } else {
        // error
        return;
    }

    WAVEFORMATEXTENSIBLE wfe = to_wave_format_extensible(p_audio_device_properties);
    result = audio_client->IsFormatSupported(
            share_mode,
            (WAVEFORMATEX *) &wfe,
            &closest_match
    );

    if (share_mode == AUDCLNT_SHAREMODE_SHARED) {
        if (result == S_OK && closest_match == nullptr) {
            // if the audio engine supports the caller-specified format,
            // IsFormatSupported sets *ppClosestMatch to NULL and returns S_OK.
        } else if (result == S_FALSE && closest_match != nullptr) {
            // not supported but alternative format available.
            LOWL_LOG_DEBUG_F("%s -> alternative format available, but not discovered via 'create_device_properties()'",
                             get_name().c_str());
        } else {
            // not supported and no alternative format
            LOWL_LOG_DEBUG_F("%s -> not supported and not alternative format", get_name().c_str());
        }
    } else if (share_mode == AUDCLNT_SHAREMODE_EXCLUSIVE) {
        if (result == S_OK && closest_match == nullptr) {
            // For exclusive mode, IsFormatSupported returns S_OK
            // if the audio endpoint device supports the caller-specified format,
            // or it returns AUDCLNT_E_UNSUPPORTED_FORMAT if the device does not support the format
        } else {
            LOWL_LOG_DEBUG_F("%s -> format not supported for exclusive mode", get_name().c_str());
        }
    }

    if (FAILED(result)) {
        switch (result) {
            case AUDCLNT_E_UNSUPPORTED_FORMAT:
                LOWL_LOG_ERROR_F("%s -> AUDCLNT_E_UNSUPPORTED_FORMAT", get_name().c_str());
                error.set_error(Lowl::ErrorCode::Error);
                break;
            case E_POINTER:
                LOWL_LOG_ERROR_F("%s -> E_POINTER", get_name().c_str());
                error.set_error(Lowl::ErrorCode::Error);
                break;
            case E_INVALIDARG:
                LOWL_LOG_ERROR_F("%s -> E_INVALIDARG", get_name().c_str());
                error.set_error(Lowl::ErrorCode::Error);
                break;
            case AUDCLNT_E_DEVICE_INVALIDATED:
                LOWL_LOG_ERROR_F("%s -> AUDCLNT_E_DEVICE_INVALIDATED", get_name().c_str());
                error.set_error(Lowl::ErrorCode::Error);
                break;
            case AUDCLNT_E_SERVICE_NOT_RUNNING:
                LOWL_LOG_ERROR_F("%s -> AUDCLNT_E_SERVICE_NOT_RUNNING", get_name().c_str());
                error.set_error(Lowl::ErrorCode::Error);
                break;
        }
        return;
    }

    result = audio_client->Initialize(
            share_mode,
            AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
            minimum_device_period,
            minimum_device_period,
            (WAVEFORMATEX *) &wfe,
            nullptr
    );
    if (FAILED(result)) {
        error.set_error(Lowl::ErrorCode::Error);
        return;
    }

    result = audio_client->GetService(
            LOWL_IID_IAudioRenderClient,
            (void **) &audio_render_client
    );
    if (FAILED(result)) {
        error.set_error(Lowl::ErrorCode::Error);
        return;
    }

    wasapi_audio_event_handle = CreateEvent(nullptr, false, false, nullptr);
    if (wasapi_audio_event_handle == INVALID_HANDLE_VALUE) {
        error.set_error(Lowl::ErrorCode::Error);
        return;
    }

    result = audio_client->SetEventHandle(wasapi_audio_event_handle);
    if (FAILED(result)) {
        error.set_error(Lowl::ErrorCode::Error);
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
        error.set_error(Lowl::ErrorCode::Error);
        return;
    }
    SetThreadPriority(wasapi_audio_thread_handle, THREAD_PRIORITY_HIGHEST);

    result = audio_client->Start();
    if (FAILED(result)) {
        error.set_error(Lowl::ErrorCode::Error);
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

        if (!audio_device_properties.exclusive_mode) {
            //share_mode == AUDCLNT_SHAREMODE_SHARED
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
    const char *device_name_ptr = WasapiDevice::wc_to_utf8(value.pwszVal);
    std::string device_name = "[" + p_driver_name + "] " + std::string(device_name_ptr);
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
    std::vector<AudioDeviceProperties> audio_device_properties = create_device_properties(
            wasapi_device,
            wave_format,
            device_name
    );
    PropVariantClear(&value);

    std::unique_ptr<WasapiDevice> device = std::make_unique<WasapiDevice>(_constructor_tag{});
    device->name = device_name;
    device->wasapi_device = wasapi_device;
    device->properties = audio_device_properties;

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
    properties.sample_rate = p_wave_format_ex->nSamplesPerSec;
    properties.channel = Lowl::Audio::get_channel(p_wave_format_ex->nChannels);
    properties.sample_format = Lowl::Audio::SampleFormat::Unknown;

    if (p_wave_format_ex->wFormatTag == WAVE_FORMAT_EXTENSIBLE) {
        const WAVEFORMATEXTENSIBLE *wave_format_extensible = (const WAVEFORMATEXTENSIBLE *) p_wave_format_ex;

        properties.wasapi.valid_bits_per_sample = wave_format_extensible->Samples.wValidBitsPerSample;

        if (IsEqualGUID(*(const GUID *) &wave_format_extensible->SubFormat,
                        *(const GUID *) &LOWL_GUID_KSDATAFORMAT_SUBTYPE_PCM)) {
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
        if (IsEqualGUID(*(const GUID *) &wave_format_extensible->SubFormat,
                        *(const GUID *) &LOWL_GUID_KSDATAFORMAT_SUBTYPE_IEEE_FLOAT)) {
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

WAVEFORMATEXTENSIBLE
Lowl::Audio::WasapiDevice::to_wave_format_extensible(
        const Lowl::Audio::AudioDeviceProperties &audio_device_properties) {
    WAVEFORMATEXTENSIBLE wfe = WAVEFORMATEXTENSIBLE();
    wfe.Format.cbSize = sizeof(wfe);
    wfe.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
    wfe.Format.nChannels = (WORD) get_channel_num(audio_device_properties.channel);
    wfe.Format.nSamplesPerSec = (DWORD) audio_device_properties.sample_rate;
    wfe.Format.wBitsPerSample = (WORD) Lowl::Audio::get_sample_bits(audio_device_properties.sample_format);
    wfe.Format.nBlockAlign = (wfe.Format.nChannels * wfe.Format.wBitsPerSample) / 8;
    wfe.Format.nAvgBytesPerSec = wfe.Format.nBlockAlign * wfe.Format.nSamplesPerSec;

    if (audio_device_properties.wasapi.valid_bits_per_sample > 0) {
        wfe.Samples.wValidBitsPerSample = audio_device_properties.wasapi.valid_bits_per_sample;
    } else {
        wfe.Samples.wValidBitsPerSample = wfe.Format.wBitsPerSample;
    }

    wfe.dwChannelMask = SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT;
    wfe.SubFormat = get_wave_sub_format(audio_device_properties.sample_format);
    return wfe;
}

std::vector<Lowl::Audio::AudioDeviceProperties>
Lowl::Audio::WasapiDevice::create_device_properties(IMMDevice *p_wasapi_device,
                                                    const WAVEFORMATEX *p_wave_format,
                                                    std::string device_name) {

    std::vector<Lowl::Audio::AudioDeviceProperties> properties = std::vector<Lowl::Audio::AudioDeviceProperties>();

    IAudioClient *tmp_audio_client = nullptr;
    HRESULT result = p_wasapi_device->Activate(
            LOWL_IID_IAudioClient3,
            CLSCTX_ALL,
            nullptr,
            (void **) &tmp_audio_client
    );
    if (FAILED(result)) {
        return properties;
    }

    AudioDeviceProperties default_properties = to_audio_device_properties(p_wave_format);

    WAVEFORMATEXTENSIBLE wfe_exclusive = to_wave_format_extensible(default_properties);
    result = tmp_audio_client->IsFormatSupported(
            AUDCLNT_SHAREMODE_EXCLUSIVE,
            (WAVEFORMATEX *) &wfe_exclusive,
            nullptr
    );
    if (result == S_OK) {
        AudioDeviceProperties property = AudioDeviceProperties();
        property.channel = default_properties.channel;
        property.sample_rate = default_properties.sample_rate;
        property.sample_format = default_properties.sample_format;
        property.exclusive_mode = true;
        properties.push_back(property);
    } else {
        switch (result) {
            case AUDCLNT_E_UNSUPPORTED_FORMAT:
                LOWL_LOG_DEBUG_F("%s -> IsFormatSupported(AUDCLNT_SHAREMODE_EXCLUSIVE): AUDCLNT_E_UNSUPPORTED_FORMAT",
                                 device_name.c_str());
                break;
            case E_POINTER:
                LOWL_LOG_DEBUG_F("%s -> IsFormatSupported(AUDCLNT_SHAREMODE_EXCLUSIVE): E_POINTER",
                                 device_name.c_str());
                break;
            case E_INVALIDARG:
                LOWL_LOG_DEBUG_F("%s -> IsFormatSupported(AUDCLNT_SHAREMODE_EXCLUSIVE): E_INVALIDARG",
                                 device_name.c_str());
                break;
            case AUDCLNT_E_DEVICE_INVALIDATED:
                LOWL_LOG_DEBUG_F("%s -> IsFormatSupported(AUDCLNT_SHAREMODE_EXCLUSIVE): AUDCLNT_E_DEVICE_INVALIDATED",
                                 device_name.c_str());
                break;
            case AUDCLNT_E_SERVICE_NOT_RUNNING:
                LOWL_LOG_DEBUG_F("%s -> IsFormatSupported(AUDCLNT_SHAREMODE_EXCLUSIVE): AUDCLNT_E_SERVICE_NOT_RUNNING",
                                 device_name.c_str());
                break;
            default:
                LOWL_LOG_DEBUG_F("%s -> IsFormatSupported(AUDCLNT_SHAREMODE_EXCLUSIVE): Unknown HRESULT(0x%lx)",
                                 device_name.c_str(), result);
                break;
        }
    }


    WAVEFORMATEXTENSIBLE wfe_shared = to_wave_format_extensible(default_properties);
    WAVEFORMATEX *closest_match = nullptr;
    memset(&closest_match, 0, sizeof(closest_match));
    result = tmp_audio_client->IsFormatSupported(
            AUDCLNT_SHAREMODE_SHARED,
            (WAVEFORMATEX *) &wfe_shared,
            &closest_match
    );
    if (result == S_OK && closest_match == nullptr) {
        AudioDeviceProperties property = AudioDeviceProperties();
        property.channel = default_properties.channel;
        property.sample_rate = default_properties.sample_rate;
        property.sample_format = default_properties.sample_format;
        property.exclusive_mode = false;
        properties.push_back(property);
    } else if (result == S_FALSE && closest_match != nullptr) {
        AudioDeviceProperties property = to_audio_device_properties(closest_match);
        property.exclusive_mode = false;
        properties.push_back(property);
    } else {
        switch (result) {
            case AUDCLNT_E_UNSUPPORTED_FORMAT:
                LOWL_LOG_DEBUG_F("%s -> IsFormatSupported(AUDCLNT_SHAREMODE_SHARED): AUDCLNT_E_UNSUPPORTED_FORMAT",
                                 device_name.c_str());
                break;
            case E_POINTER:
                LOWL_LOG_DEBUG_F("%s -> IsFormatSupported(AUDCLNT_SHAREMODE_SHARED): E_POINTER",
                                 device_name.c_str());
                break;
            case E_INVALIDARG:
                LOWL_LOG_DEBUG_F("%s -> IsFormatSupported(AUDCLNT_SHAREMODE_SHARED): E_INVALIDARG",
                                 device_name.c_str());
                break;
            case AUDCLNT_E_DEVICE_INVALIDATED:
                LOWL_LOG_DEBUG_F("%s -> IsFormatSupported(AUDCLNT_SHAREMODE_SHARED): AUDCLNT_E_DEVICE_INVALIDATED",
                                 device_name.c_str());
                break;
            case AUDCLNT_E_SERVICE_NOT_RUNNING:
                LOWL_LOG_DEBUG_F("%s -> IsFormatSupported(AUDCLNT_SHAREMODE_SHARED): AUDCLNT_E_SERVICE_NOT_RUNNING",
                                 device_name.c_str());
                break;
            default:
                LOWL_LOG_DEBUG_F("%s -> IsFormatSupported(AUDCLNT_SHAREMODE_SHARED): Unknown HRESULT(0x%lx)",
                                 device_name.c_str(), result);
                break;
        }
    }

    return properties;
}

Lowl::Audio::WasapiDevice::~WasapiDevice() {
    SAFE_RELEASE(audio_client)
    SAFE_RELEASE(wasapi_device)
    SAFE_RELEASE(audio_render_client)
    CloseHandle(wasapi_audio_event_handle);
    CloseHandle(wasapi_audio_thread_handle);
}

#endif
