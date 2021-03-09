#ifdef LOWL_DRIVER_PORTAUDIO

#include "lowl_pa_device.h"

static int audio_callback(const void *p_input_buffer, void *p_output_buffer,
                          unsigned long p_frames_per_buffer, const PaStreamCallbackTimeInfo *p_time_info,
                          PaStreamCallbackFlags p_status_flags, void *p_user_data) {
    LowlPaDevice *device = (LowlPaDevice *) p_user_data;
    return device->callback(p_input_buffer, p_output_buffer,
                            p_frames_per_buffer, p_time_info, p_status_flags
    );
}

PaStreamCallbackResult LowlPaDevice::callback(const void *p_input_buffer, void *p_output_buffer,
                                              unsigned long p_frames_per_buffer,
                                              const PaStreamCallbackTimeInfo *p_time_info,
                                              PaStreamCallbackFlags p_status_flags) {
    if (!active) {
        return paAbort;
    }

    size_t buffer_size;
    //void *buffer = audio_stream->read(buffer_size);
    // if (buffer == nullptr || buffer_size <= 0) {
    return paContinue;
    // }

    //  int bytes_requested = audio_stream->get_bytes_per_frame() * p_frames_per_buffer;
    //  if (bytes_requested <= 0) {
    //      return paContinue;
    //  }

    // uint8_t *src = (uint8_t *) buffer;
    // uint8_t *dst = (uint8_t *) p_output_buffer;
    //  return paContinue;
    //  for (int i = 0; i < bytes_to_write; i++) {
    //      dst[i] = src[buffer_position];
    //      buffer_position++;
    //  }
    //  for (int i = 0; i < bytes_to_pad; i++) {
    //      dst[i] = 0;
    //  }
//
    //  return buffer_position < buffer_size ? paContinue : paComplete;
}

void LowlPaDevice::set_stream(std::unique_ptr<LowlAudioStream> p_audio_stream, LowlError &error) {
    audio_stream = std::move(p_audio_stream);
}

void LowlPaDevice::start_stream(LowlError &error) {
    /*
    A stream is active after a successful call to Pa_StartStream(), until it
    becomes inactive either as a result of a call to Pa_StopStream() or
    Pa_AbortStream(), or as a result of a return value other than paContinue from
    the stream callback.
    */
    PaError pa_error = Pa_IsStreamActive(stream);
    if (pa_error == 1) {
        // playing
        return;
    } else if (pa_error == 0) {
        // not playing
    } else {
        // error
        error.set_error(static_cast<LowlError::Code>(pa_error));
        return;
    }

    active = true;
    pa_error = Pa_StartStream(stream);
    if (pa_error != PaErrorCode::paNoError) {
        active = false;
        error.set_error(static_cast<LowlError::Code>(pa_error));
        return;
    }
}

void LowlPaDevice::stop_stream(LowlError &error) {
    /*
    A stream is considered to be stopped prior to a successful call to
    Pa_StartStream and after a successful call to Pa_StopStream or Pa_AbortStream.
    If a stream callback returns a value other than paContinue the stream is NOT
    considered to be stopped.
    */
    PaError pa_error = Pa_IsStreamStopped(stream);
    if (pa_error == 1) {
        // stopped
        return;
    } else if (pa_error == 0) {
        // running
    } else {
        // error
        error.set_error(static_cast<LowlError::Code>(pa_error));
        return;
    }
    pa_error = Pa_StopStream(stream);
    active = false;
    if (pa_error != PaErrorCode::paNoError) {
        error.set_error(static_cast<LowlError::Code>(pa_error));
        return;
    }
}

void LowlPaDevice::open_stream(LowlError &error) {

    unsigned long frames_per_buffer = paFramesPerBufferUnspecified;
    PaStreamFlags stream_flags = paNoFlag;
    const PaDeviceInfo *device_info = Pa_GetDeviceInfo(device_index);
    if (device_info == nullptr) {
        error.set_error(LowlError::Code::Pa_GetDeviceInfo);
        return;
    }
    PaTime suggested_latency = device_info->defaultLowOutputLatency;

    PaSampleFormat sample_format = get_pa_sample_format(audio_stream->get_sample_format(), error);
    if (error.has_error()) {
        return;
    }

    const PaStreamParameters output_parameter = {
            device_index,
            audio_stream->get_channels(),
            sample_format,
            suggested_latency,
            nullptr
    };

    PaError pa_error = Pa_OpenStream(
            &stream,
            nullptr, /* no input */
            &output_parameter,
            audio_stream->get_sample_rate(),
            frames_per_buffer,
            stream_flags,
            &audio_callback,
            this);

    if (pa_error != PaErrorCode::paNoError) {
        stream = nullptr;
        error.set_error(static_cast<LowlError::Code>(pa_error));
        return;
    }
}

void LowlPaDevice::close_stream(LowlError &error) {
    PaError pa_error = Pa_CloseStream(stream);
    if (pa_error != PaErrorCode::paNoError) {
        error.set_error(static_cast<LowlError::Code>(pa_error));
        return;
    }
}

void LowlPaDevice::start(LowlError &error) {
    open_stream(error);
    if (error.has_error()) {
        return;
    }
    start_stream(error);
    if (error.has_error()) {
        return;
    }
}

void LowlPaDevice::stop(LowlError &error) {
    stop_stream(error);
    if (error.has_error()) {
        return;
    }
    close_stream(error);
    if (error.has_error()) {
        return;
    }
}

void LowlPaDevice::set_device_index(PaDeviceIndex p_device_index) {
    device_index = p_device_index;
}

LowlPaDevice::LowlPaDevice() {

}

LowlPaDevice::~LowlPaDevice() {
    PaError pa_error = Pa_CloseStream(stream);
    if (pa_error != PaErrorCode::paNoError) {
    }
    active = false;
    stream = nullptr;
}

PaSampleFormat LowlPaDevice::get_pa_sample_format(LowlSampleFormat sample_format, LowlError &error) {
    PaSampleFormat pa_sample_format;
    switch (sample_format) {
        case LowlSampleFormat::FLOAT_32: {
            pa_sample_format = paFloat32;
            break;
        }
        case LowlSampleFormat::INT_32: {
            pa_sample_format = paInt32;
            break;
        }
        case LowlSampleFormat::INT_24: {
            pa_sample_format = paInt24;
            break;
        }
        case LowlSampleFormat::INT_16: {
            pa_sample_format = paInt16;
            break;
        }
        case LowlSampleFormat::INT_8: {
            pa_sample_format = paInt8;
            break;
        }
        case LowlSampleFormat::U_INT_8: {
            pa_sample_format = paUInt8;
            break;
        }
        case LowlSampleFormat::Unknown: {
            error.set_error(LowlError::Code::PaUnknownSampleFormat);
            pa_sample_format = (PaSampleFormat) 0;
            break;
        }
    }
    return pa_sample_format;
}

#endif