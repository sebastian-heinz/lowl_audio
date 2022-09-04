#ifdef LOWL_DRIVER_PORTAUDIO

#include "lowl_audio_pa_device.h"

#include "lowl_logger.h"

#include <algorithm>

#ifdef PA_USE_WASAPI
#include <pa_win_wasapi.h>
#include <core/os/memory.h>
#endif

static int audio_callback(const void *p_input_buffer, void *p_output_buffer,
                          unsigned long p_frames_per_buffer, const PaStreamCallbackTimeInfo *p_time_info,
                          PaStreamCallbackFlags p_status_flags, void *p_user_data) {
    Lowl::Audio::AudioDevicePa *device = (Lowl::Audio::AudioDevicePa *) p_user_data;
    return device->callback(p_input_buffer, p_output_buffer,
                            p_frames_per_buffer, p_time_info, p_status_flags
    );
}

PaStreamCallbackResult Lowl::Audio::AudioDevicePa::callback(const void *p_input_buffer, void *p_output_buffer,
                                                     unsigned long p_frames_per_buffer,
                                                     const PaStreamCallbackTimeInfo *p_time_info,
                                                     PaStreamCallbackFlags p_status_flags) {
    if (!active) {
        return paAbort;
    }

    float *dst = (float *) p_output_buffer;
    unsigned long current_frame = 0;
    AudioFrame frame{};
    for (; current_frame < p_frames_per_buffer; current_frame++) {
        AudioSource::ReadResult read_result = audio_source->read(frame);
        if (read_result == AudioSource::ReadResult::Read) {
            for (int current_channel = 0; current_channel < audio_source->get_channel_num(); current_channel++) {
                std::clamp(frame[current_channel], AudioFrame::MIN_SAMPLE_VALUE, AudioFrame::MAX_SAMPLE_VALUE);
                *dst++ = (float) frame[current_channel];
            }
        } else if (read_result == AudioSource::ReadResult::End) {
            break;
        } else if (read_result == AudioSource::ReadResult::Pause) {
            break;
        } else if (read_result == AudioSource::ReadResult::Remove) {
            break;
        }
    }

    if (current_frame < p_frames_per_buffer) {

        // fill buffer with silence if not enough samples available.
        unsigned long missing_frames = p_frames_per_buffer - current_frame;
        unsigned long missing_samples = missing_frames * (unsigned long) audio_source->get_channel_num();

        // TODO check if this is correct
        //memset(&dst[0], 0, missing_samples);
        unsigned long current_sample = 0;
        for (; current_sample < missing_samples; current_sample++) {
            *dst++ = 0;
        }
    }

    double time_request_ms = ((double) p_frames_per_buffer / audio_source->get_sample_rate()) * 1000;
    return paContinue;
}

void Lowl::Audio::AudioDevicePa::start_stream(Error &error) {
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
        error.set_vendor_error(pa_error, Error::VendorError::PortAudioVendorError);
        return;
    }

    active = true;
    pa_error = Pa_StartStream(stream);
    if (pa_error != PaErrorCode::paNoError) {
        active = false;
        error.set_vendor_error(pa_error, Error::VendorError::PortAudioVendorError);
        return;
    }
}

void Lowl::Audio::AudioDevicePa::stop_stream(Error &error) {
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
        error.set_vendor_error(pa_error, Error::VendorError::PortAudioVendorError);
        return;
    }
    pa_error = Pa_StopStream(stream);

    if (pa_error != PaErrorCode::paNoError) {
        error.set_vendor_error(pa_error, Error::VendorError::PortAudioVendorError);
        return;
    }
}

void Lowl::Audio::AudioDevicePa::open_stream(Error &error) {

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
        error.set_vendor_error(pa_error, Error::VendorError::PortAudioVendorError);
        return;
    }
}

void Lowl::Audio::AudioDevicePa::close_stream(Error &error) {
    PaError pa_error = Pa_CloseStream(stream);
    if (pa_error != PaErrorCode::paNoError) {
        error.set_vendor_error(pa_error, Error::VendorError::PortAudioVendorError);
        return;
    }
}

PaStreamParameters
Lowl::Audio::AudioDevicePa::create_output_parameters(Lowl::Audio::AudioChannel p_channel, Lowl::Audio::SampleFormat p_sample_format,
                                              Error &error) {
    const PaDeviceInfo *device_info = Pa_GetDeviceInfo(device_index);
    if (device_info == nullptr) {
        error.set_error(ErrorCode::PortAudioNoDeviceInfo);
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

void Lowl::Audio::AudioDevicePa::start(std::shared_ptr<AudioSource> p_audio_source, Lowl::Error &error) {
    audio_source = p_audio_source;
    start(error);
}

void Lowl::Audio::AudioDevicePa::start(Error &error) {
    open_stream(error);
    if (error.has_error()) {
        return;
    }
    start_stream(error);
    if (error.has_error()) {
        return;
    }
}

void Lowl::Audio::AudioDevicePa::stop(Error &error) {
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

void Lowl::Audio::AudioDevicePa::set_device_index(PaDeviceIndex p_device_index) {
    device_index = p_device_index;
}

Lowl::Audio::AudioDevicePa::AudioDevicePa() {
    active = false;
    stream = nullptr;
    device_index = paNoDevice;
}

Lowl::Audio::AudioDevicePa::~AudioDevicePa() {
    PaError pa_error = Pa_CloseStream(stream);
    if (pa_error != PaErrorCode::paNoError) {
    }
    device_index = paNoDevice;
    active = false;
    stream = nullptr;
}

PaSampleFormat Lowl::Audio::AudioDevicePa::get_pa_sample_format(Lowl::Audio::SampleFormat sample_format, Lowl::Error &error) {
    PaSampleFormat pa_sample_format;
    switch (sample_format) {
        case Lowl::Audio::SampleFormat::FLOAT_32: {
            pa_sample_format = paFloat32;
            break;
        }
        case Lowl::Audio::SampleFormat::INT_32: {
            pa_sample_format = paInt32;
            break;
        }
        case Lowl::Audio::SampleFormat::INT_24: {
            pa_sample_format = paInt24;
            break;
        }
        case Lowl::Audio::SampleFormat::INT_16: {
            pa_sample_format = paInt16;
            break;
        }
        case Lowl::Audio::SampleFormat::INT_8: {
            pa_sample_format = paInt8;
            break;
        }
        case Lowl::Audio::SampleFormat::U_INT_8: {
            pa_sample_format = paUInt8;
            break;
        }
        case Lowl::Audio::SampleFormat::FLOAT_64: {
            error.set_error(ErrorCode::PortAudioUnknownSampleFormat);
            pa_sample_format = (PaSampleFormat) 0;
            break;
        }
        case Lowl::Audio::SampleFormat::Unknown: {
            error.set_error(ErrorCode::PortAudioUnknownSampleFormat);
            pa_sample_format = (PaSampleFormat) 0;
            break;
        }
    }
    return pa_sample_format;
}

Lowl::SampleRate Lowl::Audio::AudioDevicePa::get_default_sample_rate() {
    const PaDeviceInfo *device_info = Pa_GetDeviceInfo(device_index);
    if (device_info == nullptr) {
        return 0;
    }
    Lowl::SampleRate sample_rate = device_info->defaultSampleRate;
    return sample_rate;
}


#endif
