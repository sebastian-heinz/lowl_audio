#ifndef LOWL_AUDIO_WASAPI_DEVICE_H
#define LOWL_AUDIO_WASAPI_DEVICE_H

#ifdef LOWL_DRIVER_WASAPI

#include "audio/lowl_audio_device.h"

namespace Lowl::Audio {

    class WasapiDevice : public AudioDevice {

    private:
        static char *wc_to_utf8(const wchar_t *p_wc);
        void *wasapi_device;
        Lowl::SampleRate default_sample_rate;
        Lowl::Audio::AudioChannel output_channel;
        Lowl::Audio::SampleFormat sample_format;

    public:
        static std::unique_ptr<WasapiDevice> construct(
                const std::string &p_driver_name,
                void *p_wasapi_device,
                Error &error
        );

        uint32_t audio_callback();

        void start(std::shared_ptr<AudioSource> p_audio_source, Error &error) override;

        void stop(Error &error) override;

        bool is_supported(Lowl::Audio::AudioChannel channel,
                          Lowl::SampleRate sample_rate,
                          SampleFormat sample_format,
                          Error &error
        ) override;

        Lowl::SampleRate get_default_sample_rate() override;

        void set_exclusive_mode(bool p_exclusive_mode, Error &error) override;

        WasapiDevice();

        ~WasapiDevice();
    };
} //namespace Lowl::Audio

#endif /* LOWL_DRIVER_WASAPI */
#endif /* LOWL_AUDIO_WASAPI_DEVICE_H */
