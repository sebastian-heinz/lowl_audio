#ifndef LOWL_AUDIO_WASAPI_DEVICE_H
#define LOWL_AUDIO_WASAPI_DEVICE_H

#ifdef LOWL_DRIVER_WASAPI

#include "audio/lowl_audio_device.h"

#include <mmdeviceapi.h>
#include <audioclient.h>

#include <vector>

namespace Lowl::Audio {

    class WasapiDevice : public AudioDevice {

    private:
        static char *wc_to_utf8(const wchar_t *p_wc);

        static Lowl::Audio::AudioDeviceProperties to_audio_device_properties(const WAVEFORMATEX *p_wave_format_ex);

        static WAVEFORMATEXTENSIBLE
        to_wave_format_extensible(const Lowl::Audio::AudioDeviceProperties &p_wave_format_ex);

        static GUID get_wave_sub_format(const Lowl::Audio::SampleFormat p_sample_format);

        static std::vector<Lowl::Audio::AudioDeviceProperties> create_device_properties(
                IMMDevice *p_wasapi_device,
                const WAVEFORMATEX *wave_format,
                std::string device_name
        );


        IMMDevice *wasapi_device;
        IAudioClient *audio_client;
        IAudioRenderClient *audio_render_client;
        HANDLE wasapi_audio_thread_handle;
        HANDLE wasapi_audio_event_handle;

        AudioDeviceProperties audio_device_properties{};


    public:
        static std::unique_ptr<WasapiDevice> construct(
                const std::string &p_driver_name,
                void *p_wasapi_device,
                Error &error
        );

        uint32_t audio_callback();

        virtual void start(AudioDeviceProperties p_audio_device_properties,
                           std::shared_ptr<AudioSource> p_audio_source,
                           Error &error) override;

        virtual void stop(Error &error) override;

        WasapiDevice(_constructor_tag);

        ~WasapiDevice() override;
    };
}

#endif /* LOWL_DRIVER_WASAPI */
#endif /* LOWL_AUDIO_WASAPI_DEVICE_H */
