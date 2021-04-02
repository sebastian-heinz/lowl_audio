#ifdef LOWL_DRIVER_PORTAUDIO

#include "lowl_device_pa.h"

#ifdef LOWL_PROFILING

#include <chrono>

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
#ifdef LOWL_PROFILING
    auto t1 = std::chrono::high_resolution_clock::now();
#endif
    if (!active) {
        return paAbort;
    }

    float *dst = (float *) p_output_buffer;
    unsigned long current_frame = 0;
    for (; current_frame < p_frames_per_buffer; current_frame++) {
        if (audio_mixer) {
            if (!audio_mixer->mix_next_frame()) {
                // todo couldn't mix
            }
        }
        AudioFrame frame;
        if (!audio_stream->read(frame)) {
            // stream empty
            break;
        }
        for (int current_channel = 0; current_channel < audio_stream->get_channel_num(); current_channel++) {
            *dst++ = frame[current_channel];
        }
    }

    if (current_frame < p_frames_per_buffer) {
        // fill buffer with silence if not enough samples available.
        unsigned long missing_frames = p_frames_per_buffer - current_frame;
        unsigned long missing_samples = missing_frames * audio_stream->get_channel_num();
        unsigned long current_sample = 0;
        for (; current_sample < missing_samples; current_sample++) {
            *dst++ = 0;
        }
    }

#ifdef LOWL_PROFILING
    auto t2 = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> ms_double = t2 - t1;
    double duration = ms_double.count();

    if (callback_max_duration < duration) {
        callback_max_duration = duration;
    }
    if (callback_min_duration > duration) {
        callback_min_duration = duration;
    }
    callback_total_duration += duration;
    callback_count++;
    callback_avg_duration = callback_total_duration / callback_count;
    time_request_ms = (p_frames_per_buffer / audio_stream->get_sample_rate()) * 1000;
#endif

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
    active = false;
    if (pa_error != PaErrorCode::paNoError) {
        error.set_error(static_cast<ErrorCode>(pa_error));
        return;
    }
}

void Lowl::PaDevice::open_stream(Error &error) {

    unsigned long frames_per_buffer = paFramesPerBufferUnspecified;
    PaStreamFlags stream_flags = paNoFlag;
    const PaDeviceInfo *device_info = Pa_GetDeviceInfo(device_index);
    if (device_info == nullptr) {
        error.set_error(ErrorCode::Pa_GetDeviceInfo);
        return;
    }
    PaTime suggested_latency = device_info->defaultLowOutputLatency;

    PaSampleFormat sample_format = get_pa_sample_format(audio_stream->get_sample_format(), error);
    if (error.has_error()) {
        return;
    }

    const PaStreamParameters output_parameter = {
            device_index,
            (int) audio_stream->get_channel(),
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

void Lowl::PaDevice::start_stream(std::shared_ptr<AudioStream> p_audio_stream, Lowl::Error &error) {
    audio_mixer = nullptr;
    audio_stream = p_audio_stream;
    start(error);
}

void Lowl::PaDevice::start_mixer(std::shared_ptr<AudioMixer> p_audio_mixer, Lowl::Error &error) {
    audio_mixer = p_audio_mixer;
    audio_stream = audio_mixer->get_out_stream();
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
#ifdef LOWL_PROFILING
    callback_count = 0;
    callback_total_duration = 0;
    callback_max_duration = 0;
    callback_min_duration = std::numeric_limits<double>::max();
    callback_avg_duration = 0;
    time_request_ms = 0;
#endif
}

Lowl::PaDevice::~PaDevice() {
    PaError pa_error = Pa_CloseStream(stream);
    if (pa_error != PaErrorCode::paNoError) {
    }
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

#endif