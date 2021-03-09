#ifndef LOWL_PA_DEVICE_H
#define LOWL_PA_DEVICE_H

#ifdef LOWL_DRIVER_PORTAUDIO

#include <portaudio.h>

#include "../../lowl_device.h"
#include "../../lowl_sample_format.h"
#include "../../lowl_error.h"

class LowlPaDevice : public Lowl::Device {

private:
    PaDeviceIndex device_index;
    PaStream *stream;
    bool active;
    std::unique_ptr<LowlAudioStream> audio_stream;

    void start_stream(Lowl::LowlError &error);

    void stop_stream(Lowl::LowlError &error);

    void open_stream(Lowl::LowlError &error);

    void close_stream(Lowl::LowlError &error);

    PaSampleFormat get_pa_sample_format(Lowl::SampleFormat sample_format, Lowl::LowlError &error);

public:
    virtual void start(Lowl::LowlError &error) override;

    virtual void stop(Lowl::LowlError &error) override;

    virtual void set_stream(std::unique_ptr<LowlAudioStream> p_audio_stream, Lowl::LowlError &error) override;

public:
    PaStreamCallbackResult callback(const void *p_input_buffer,
                                    void *p_output_buffer,
                                    unsigned long p_frames_per_buffer,
                                    const PaStreamCallbackTimeInfo *p_time_info,
                                    PaStreamCallbackFlags p_status_flags
    );

    void set_device_index(PaDeviceIndex device_index);

    LowlPaDevice();

    ~LowlPaDevice();
};

#endif /* LOWL_DRIVER_PORTAUDIO */
#endif /* LOWL_PA_DEVICE_H */
