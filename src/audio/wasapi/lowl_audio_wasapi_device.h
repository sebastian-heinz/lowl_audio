#ifndef LOWL_AUDIO_WASAPI_DEVICE_H
#define LOWL_AUDIO_WASAPI_DEVICE_H

#ifdef LOWL_DRIVER_WASAPI

#include "audio/lowl_audio_device.h"

#include <mmdeviceapi.h>
#include <audioclient.h>

namespace Lowl::Audio {

    class WasapiDevice : public AudioDevice {

    private:
        static char *wc_to_utf8(const wchar_t *p_wc);

        static Lowl::Audio::SampleFormat get_sample_format(const WAVEFORMATEX *p_wave_format_ex);

        static GUID get_wave_sub_format(const Lowl::Audio::SampleFormat p_sample_format);

        IMMDevice *wasapi_device;
        IAudioClient *audio_client;
        IAudioRenderClient *audio_render_client;
        HANDLE wasapi_audio_thread_handle;
        HANDLE wasapi_audio_event_handle;
        AUDCLNT_SHAREMODE share_mode;

        Lowl::SampleRate default_sample_rate;
        Lowl::Audio::AudioChannel output_channel;
        Lowl::Audio::SampleFormat sample_format;

        void populate_device_properties();

    public:
        static std::unique_ptr<WasapiDevice> construct(
                const std::string &p_driver_name,
                void *p_wasapi_device,
                Error &error
        );


        uint32_t audio_callback();

        void start(std::shared_ptr<AudioSource> p_audio_source, Error &error) override;

        void stop(Error &error) override;

        WasapiDevice();

        ~WasapiDevice();
    };
} //namespace Lowl::Audio

#endif /* LOWL_DRIVER_WASAPI */
#endif /* LOWL_AUDIO_WASAPI_DEVICE_H */
