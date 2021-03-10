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

        void start_stream(LowlError &error);

        void stop_stream(LowlError &error);

        void open_stream(LowlError &error);

        void close_stream(LowlError &error);

        PaSampleFormat get_pa_sample_format(SampleFormat sample_format, LowlError &error);

    public:
        virtual void start(LowlError &error) override;

        virtual void stop(LowlError &error) override;

        virtual void set_stream(std::unique_ptr<AudioStream> p_audio_stream, LowlError &error) override;

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
