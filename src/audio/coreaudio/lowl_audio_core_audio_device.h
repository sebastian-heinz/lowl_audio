#ifndef LOWL_AUDIO_DEVICE_CORE_AUDIO_H
#define LOWL_AUDIO_DEVICE_CORE_AUDIO_H

#ifdef LOWL_DRIVER_CORE_AUDIO

#include "audio/lowl_audio_device.h"

#include <CoreAudio/AudioHardware.h>

namespace Lowl::Audio {

    class CoreAudioDevice : public AudioDevice {

    private:
        AudioObjectID device_id;
        Lowl::SampleRate default_sample_rate;
        Lowl::Audio::AudioChannel output_channel;
        uint32_t input_stream_count;
        uint32_t output_stream_count;

        CoreAudioDevice();

    public:
        static std::unique_ptr<CoreAudioDevice>
        construct(const std::string &p_driver_name, AudioObjectID p_device_id, Error &error);

        void start(std::shared_ptr<AudioSource> p_audio_source, Error &error) override;

        void stop(Error &error) override;

        bool is_supported(Lowl::Audio::AudioChannel channel,
                          Lowl::SampleRate sample_rate,
                          SampleFormat sample_format,
                          Error &error
        ) override;

        Lowl::SampleRate get_default_sample_rate() override;

        void set_exclusive_mode(bool p_exclusive_mode, Error &error) override;

        ~CoreAudioDevice();
    };
}

#endif /* LOWL_DRIVER_CORE_AUDIO */
#endif /* LOWL_AUDIO_DEVICE_CORE_AUDIO_H */
