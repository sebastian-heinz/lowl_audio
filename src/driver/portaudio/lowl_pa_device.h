#ifndef LOWL_PA_DEVICE_H
#define LOWL_PA_DEVICE_H

#ifdef LOWL_DRIVER_PORTAUDIO

#include "../../lowl_device.h"

#include <portaudio.h>

namespace Lowl {

    class PaDevice : public Device {

    private:
        PaDeviceIndex device_index;
        PaStream *stream;
        bool active;
        std::unique_ptr<AudioStream> audio_stream;

        void start_stream(Error &error);

        void stop_stream(Error &error);

        void open_stream(Error &error);

        void close_stream(Error &error);

        PaSampleFormat get_pa_sample_format(SampleFormat sample_format, Error &error);

    public:
        virtual void start(Error &error) override;

        virtual void stop(Error &error) override;

        virtual void set_stream(std::unique_ptr<AudioStream> p_audio_stream, Error &error) override;

    public:
        PaStreamCallbackResult callback(const void *p_input_buffer,
                                        void *p_output_buffer,
                                        unsigned long p_frames_per_buffer,
                                        const PaStreamCallbackTimeInfo *p_time_info,
                                        PaStreamCallbackFlags p_status_flags
        );

        void set_device_index(PaDeviceIndex device_index);

        PaDevice();

        ~PaDevice();
    };
}

#endif /* LOWL_DRIVER_PORTAUDIO */
#endif /* LOWL_PA_DEVICE_H */
