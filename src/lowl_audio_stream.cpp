#include "../include/lowl_audio_stream.h"

LowlAudioStream::LowlAudioStream() {
    initialized = false;
    sample_format = LowlSampleFormat::Unknown;
    sample_rate = 0;
    channels = 0;
    sample_size = 0;
    bytes_per_frame = 0;
    buffer = nullptr;
}

LowlAudioStream::~LowlAudioStream() {
    if (buffer != nullptr) {
        delete buffer;
    }
}

void LowlAudioStream::initialize(LowlSampleFormat p_sample_format, double p_sample_rate, int p_channels, LowlError &error) {
    if (initialized) {
        error.set_error(LowlError::Code::AudioStreamAlreadyInitialized);
        return;
    }

    sample_format = p_sample_format;
    sample_size = get_sample_size(sample_format);
    sample_rate = p_sample_rate;
    channels = p_channels;

    bytes_per_frame = channels * sample_size;


    int frames_per_buffer = 15;
    int time_request_seconds = frames_per_buffer / sample_rate;
    buffer = new moodycamel::BlockingReaderWriterCircularBuffer<Frame>(10);

    initialized = true;
}

int LowlAudioStream::get_channels() const {
    return channels;
}

double LowlAudioStream::get_sample_rate() const {
    return sample_rate;
}

LowlSampleFormat LowlAudioStream::get_sample_format() const {
    return sample_format;
}

inline int LowlAudioStream::get_sample_size(LowlSampleFormat format) {
    switch (format) {
        case LowlSampleFormat::FLOAT_32:
        case LowlSampleFormat::INT_32:
            return 4;
        case LowlSampleFormat::INT_24:
            return 3;
        case LowlSampleFormat::INT_16:
            return 2;
        case LowlSampleFormat::INT_8:
        case LowlSampleFormat::U_INT_8:
            return 1;
        case LowlSampleFormat::Unknown:
            return 0;
    }
}

LowlAudioStream::Frame LowlAudioStream::read(size_t &length) const {
    Frame frame;
    if (!buffer->try_dequeue(frame)) {
        return frame;
    }
    return frame;
}

void LowlAudioStream::write(void *data, size_t length) {
    if (length <= 0) {
        return;
    }
    int frames_num = length / bytes_per_frame;
    int frames_size = frames_num * bytes_per_frame;
    if (frames_size != length) {
        // incomplete frames
    }

    for(int i =0; i < frames_num; i++){
        Frame frame;
        if (!buffer->try_enqueue(frame)) {
            return;
        }
    }


}

int LowlAudioStream::get_bytes_per_frame() const {
    return bytes_per_frame;
}


