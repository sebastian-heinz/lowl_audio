#ifdef LOWL_DRIVER_PORTAUDIO

#include "lowl_device_pa.h"
#include "lowl_logger.h"

#ifdef PA_USE_WASAPI
#include <pa_win_wasapi.h>
#endif

static int audio_callback(const void *p_input_buffer, void *p_output_buffer,
                          unsigned long p_frames_per_buffer, const PaStreamCallbackTimeInfo *p_time_info,
                          PaStreamCallbackFlags p_status_flags, void *p_user_data) {
    Lowl::PaDevice *device = (Lowl::PaDevice *) p_user_data;
    return device->callback(p_input_buffer, p_output_buffer,
                            p_frames_per_buffer, p_time_info, p_status_flags
    );
}

PaStreamCallbackResult Lowl::PaDevice::callback(const void *p_input_buffer, void *p_output_buffer,
                                                unsigned long p_frames_per_buffer,
                                                const PaStreamCallbackTimeInfo *p_time_info,
                                                PaStreamCallbackFlags p_status_flags) {
    if (!active) {
        return paAbort;
    }

    float *dst = (float *) p_output_buffer;
    unsigned long current_frame = 0;
    for (; current_frame < p_frames_per_buffer; current_frame++) {
        AudioFrame frame{};
        if (!audio_source->read(frame)) {
            // stream empty
            break;
        }
        for (int current_channel = 0; current_channel < audio_source->get_channel_num(); current_channel++) {
            if (frame[current_channel] > 1.0) {
                frame[current_channel] = 1.0;
            }
            if (frame[current_channel] < -1.0) {
                frame[current_channel] = -1.0;
            }
            *dst++ = frame[current_channel];
        }
    }

    if (current_frame < p_frames_per_buffer) {

        // fill buffer with silence if not enough samples available.
        unsigned long missing_frames = p_frames_per_buffer - current_frame;
        unsigned long missing_samples = missing_frames * audio_source->get_channel_num();

        // TODO check if this is correct
        //memset(&dst[0], 0, missing_samples);
        unsigned long current_sample = 0;
        for (; current_sample < missing_samples; current_sample++) {
            *dst++ = 0;
        }
    }

    double time_request_ms = (p_frames_per_buffer / audio_source->get_sample_rate()) * 1000;
    return paContinue;
}

void Lowl::PaDevice::start_stream(Error &error) {
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
        error.set_error(static_cast<ErrorCode>(pa_error));
        return;
    }

    active = true;
    pa_error = Pa_StartStream(stream);
    if (pa_error != PaErrorCode::paNoError) {
        active = false;
        error.set_error(static_cast<ErrorCode>(pa_error));
        return;
    }
}

void Lowl::PaDevice::stop_stream(Error &error) {
    /*
    A stream is considered to be stopped prior to a successful call to
    Pa_StartStream and after a successful call to Pa_StopStream or Pa_AbortStream.
    If a stream callback returns a value other than paContinue the stream is NOT
    considered to be stopped.
    */
    active = false;
    PaError pa_error = Pa_IsStreamStopped(stream);
    if (pa_error == 1) {
        // stopped
        return;
    } else if (pa_error == 0) {
        // running
    } else {
        // error
        error.set_error(static_cast<ErrorCode>(pa_error));
        return;
    }
    pa_error = Pa_StopStream(stream);

    if (pa_error != PaErrorCode::paNoError) {
        error.set_error(static_cast<ErrorCode>(pa_error));
        return;
    }
}

void Lowl::PaDevice::open_stream(Error &error) {

    unsigned long frames_per_buffer = paFramesPerBufferUnspecified;
    PaStreamFlags stream_flags = paNoFlag;

    PaStreamParameters output_parameter = create_output_parameters(
            audio_source->get_channel(), audio_source->get_sample_format(), error
    );
    if (error.has_error()) {
        return;
    }

    if (exclusive_mode) {
        enable_exclusive_mode(output_parameter, error);
        if (error.has_error()) {
            return;
        }
    }

    PaError pa_error = Pa_OpenStream(
            &stream,
            nullptr, /* no input */
            &output_parameter,
            audio_source->get_sample_rate(),
            frames_per_buffer,
            stream_flags,
            &audio_callback,
            this);

    if (pa_error != PaErrorCode::paNoError) {
        stream = nullptr;
        error.set_error(static_cast<ErrorCode>(pa_error));
        return;
    }
}

void Lowl::PaDevice::close_stream(Error &error) {
    PaError pa_error = Pa_CloseStream(stream);
    if (pa_error != PaErrorCode::paNoError) {
        error.set_error(static_cast<ErrorCode>(pa_error));
        return;
    }
}

bool Lowl::PaDevice::is_supported(Lowl::Channel p_channel, Lowl::SampleRate p_sample_rate,
                                  Lowl::SampleFormat p_sample_format, Error &error) {
    const PaStreamParameters output_parameter = create_output_parameters(p_channel, p_sample_format, error);
    if (error.has_error()) {
        return false;
    }

    PaError pa_error = Pa_IsFormatSupported(nullptr, &output_parameter, p_sample_rate);
    if (pa_error != PaErrorCode::paNoError) {
        error.set_error(static_cast<ErrorCode>(pa_error));
        return false;
    }
    return true;
}

PaStreamParameters
Lowl::PaDevice::create_output_parameters(Lowl::Channel p_channel, Lowl::SampleFormat p_sample_format, Error &error) {
    const PaDeviceInfo *device_info = Pa_GetDeviceInfo(device_index);
    if (device_info == nullptr) {
        error.set_error(ErrorCode::Pa_GetDeviceInfo);
        return PaStreamParameters{};
    }
    PaTime suggested_latency = device_info->defaultLowOutputLatency;

    PaSampleFormat sample_format = get_pa_sample_format(p_sample_format, error);
    if (error.has_error()) {
        return PaStreamParameters{};
    }

    const PaStreamParameters output_parameter = {
            device_index,
            (int) p_channel,
            sample_format,
            suggested_latency,
            nullptr
    };
    return output_parameter;
}

void Lowl::PaDevice::start(std::shared_ptr<AudioSource> p_audio_source, Lowl::Error &error) {
    audio_source = p_audio_source;
    start(error);
}

void Lowl::PaDevice::start(Error &error) {
    open_stream(error);
    if (error.has_error()) {
        return;
    }
    start_stream(error);
    if (error.has_error()) {
        return;
    }
}

void Lowl::PaDevice::stop(Error &error) {
    stop_stream(error);
    audio_source = nullptr;
    if (error.has_error()) {
        return;
    }
    close_stream(error);
    if (error.has_error()) {
        return;
    }
}

void Lowl::PaDevice::set_device_index(PaDeviceIndex p_device_index) {
    device_index = p_device_index;
}

Lowl::PaDevice::PaDevice() {
    active = false;
    stream = nullptr;
}

Lowl::PaDevice::~PaDevice() {
    PaError pa_error = Pa_CloseStream(stream);
    if (pa_error != PaErrorCode::paNoError) {
    }
    device_index = paNoDevice;
    active = false;
    stream = nullptr;
}

PaSampleFormat Lowl::PaDevice::get_pa_sample_format(Lowl::SampleFormat sample_format, Lowl::Error &error) {
    PaSampleFormat pa_sample_format;
    switch (sample_format) {
        case Lowl::SampleFormat::FLOAT_32: {
            pa_sample_format = paFloat32;
            break;
        }
        case Lowl::SampleFormat::INT_32: {
            pa_sample_format = paInt32;
            break;
        }
        case Lowl::SampleFormat::INT_24: {
            pa_sample_format = paInt24;
            break;
        }
        case Lowl::SampleFormat::INT_16: {
            pa_sample_format = paInt16;
            break;
        }
        case Lowl::SampleFormat::INT_8: {
            pa_sample_format = paInt8;
            break;
        }
        case Lowl::SampleFormat::U_INT_8: {
            pa_sample_format = paUInt8;
            break;
        }
        case Lowl::SampleFormat::Unknown: {
            error.set_error(ErrorCode::PaUnknownSampleFormat);
            pa_sample_format = (PaSampleFormat) 0;
            break;
        }
    }
    return pa_sample_format;
}

Lowl::SampleRate Lowl::PaDevice::get_default_sample_rate() {
    const PaDeviceInfo *device_info = Pa_GetDeviceInfo(device_index);
    if (device_info == nullptr) {
        return 0;
    }
    Lowl::SampleRate sample_rate = device_info->defaultSampleRate;
    return sample_rate;
}

void Lowl::PaDevice::set_exclusive_mode(bool p_exclusive_mode, Lowl::Error &error) {
    if (p_exclusive_mode) {
        if (active) {
            // stream already running
            error.set_error(ErrorCode::Error);
            return;
        }
        exclusive_mode = true;
        return;
    } else {
        exclusive_mode = false;
    }
}

void Lowl::PaDevice::enable_exclusive_mode(PaStreamParameters &stream_parameters, Lowl::Error &error) {
    if (stream_parameters.device != device_index) {
        // parameter mismatch this device
        error.set_error(ErrorCode::Error);
        return;
    }
    const PaDeviceInfo *pa_device_info = Pa_GetDeviceInfo(device_index);
    if (pa_device_info == nullptr) {
        // unknown device
        error.set_error(ErrorCode::Error);
        return;
    }
    const PaHostApiInfo *pa_api_info = Pa_GetHostApiInfo(pa_device_info->hostApi);
    if (pa_api_info == nullptr) {
        // unknown host api
        error.set_error(ErrorCode::Error);
        return;
    }
    bool exclusive_mode_applied = false;
#ifdef PA_USE_WASAPI
    if (pa_api_info->type == paWASAPI) {
        PaWasapiStreamInfo *wasapiInfo = (PaWasapiStreamInfo *) memalloc(sizeof(PaWasapiStreamInfo));
        wasapiInfo->size = sizeof(PaWasapiStreamInfo);
        wasapiInfo->hostApiType = paWASAPI;
        wasapiInfo->version = 1;
        wasapiInfo->flags = (paWinWasapiExclusive | paWinWasapiThreadPriority);
        wasapiInfo->threadPriority = eThreadPriorityProAudio;
        p_stream_parameter->set_host_api_specific_stream_info(wasapiInfo);
        exclusive_mode_applied = true;
    }
#endif
    if (!exclusive_mode_applied) {
        Logger::log(Logger::Level::Warn, "!exclusive_mode_applied");
    }
}

#endif
