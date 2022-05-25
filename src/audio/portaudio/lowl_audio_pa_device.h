#ifndef LOWL_AUDIO_PA_DEVICE_H
#define LOWL_AUDIO_PA_DEVICE_H

#ifdef LOWL_DRIVER_PORTAUDIO

#include "audio/lowl_audio_device.h"

#include <portaudio.h>

namespace Lowl::Audio {

    class AudioDevicePa : public AudioDevice {

    private:
        PaDeviceIndex device_index;
        PaStream *stream;
        bool active;

        void start(Error &error);

        void start_stream(Error &error);

        void stop_stream(Error &error);

        void open_stream(Error &error);

        void close_stream(Error &error);

        PaStreamParameters
        create_output_parameters(Lowl::Audio::AudioChannel p_channel, Lowl::Audio::SampleFormat p_sample_format, Error &error);

        PaSampleFormat get_pa_sample_format(SampleFormat sample_format, Error &error);

        void enable_exclusive_mode(PaStreamParameters &stream_parameters, Error &error);

    public:
        virtual void start(std::shared_ptr<AudioSource> p_audio_source, Error &error) override;

        virtual void stop(Error &error) override;

        virtual bool is_supported(Lowl::Audio::AudioChannel channel, Lowl::SampleRate sample_rate, Lowl::Audio::SampleFormat sample_format,
                                  Error &error) override;

        virtual Lowl::SampleRate get_default_sample_rate() override;

        virtual void set_exclusive_mode(bool p_exclusive_mode, Error &error) override;

        PaStreamCallbackResult callback(const void *p_input_buffer,
                                        void *p_output_buffer,
                                        unsigned long p_frames_per_buffer,
                                        const PaStreamCallbackTimeInfo *p_time_info,
                                        PaStreamCallbackFlags p_status_flags
        );

        void set_device_index(PaDeviceIndex device_index);

        AudioDevicePa();

        ~AudioDevicePa();
    };
}

#endif /* LOWL_DRIVER_PORTAUDIO */
#endif /* LOWL_AUDIO_PA_DEVICE_H */
