#ifndef LOWL_PA_DEVICE_H
#define LOWL_PA_DEVICE_H

#ifdef LOWL_DRIVER_PORTAUDIO

#include <portaudio.h>

#include "../include/lowl_device.h"

class LowlPaDevice : public LowlDevice {

private:
    PaDeviceIndex device_index;
    PaStream *stream;
    bool active;
    std::unique_ptr<LowlAudioStream> audio_stream;

    void start_stream(LowlError &error);

    void stop_stream(LowlError &error);

    void open_stream(LowlError &error);

    void close_stream(LowlError &error);

    PaSampleFormat get_pa_sample_format(LowlSampleFormat sample_format, LowlError &error);

public:
    virtual void start(LowlError &error) override;

    virtual void stop(LowlError &error) override;

    virtual void set_stream(std::unique_ptr<LowlAudioStream> p_audio_stream, LowlError &error) override;

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
