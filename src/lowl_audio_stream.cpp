#include "lowl_audio_stream.h"
#include "lowl_sample_format.h"
#include "lowl_audio_frame.h"
#include "lowl_error.h"

LowlAudioStream::LowlAudioStream() {
    initialized = false;
    sample_format = Lowl::SampleFormat::Unknown;
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

void LowlAudioStream::initialize(Lowl::SampleFormat p_sample_format, double p_sample_rate, int p_channels, Lowl::LowlError &error) {
    if (initialized) {
        error.set_error(Lowl::LowlError::Code::AudioStreamAlreadyInitialized);
        return;
    }

    sample_format = p_sample_format;
    sample_size = Lowl::get_sample_size(sample_format);
    sample_rate = p_sample_rate;
    channels = p_channels;

    bytes_per_frame = channels * sample_size;


    int frames_per_buffer = 15;
    int time_request_seconds = frames_per_buffer / sample_rate;
    buffer = new moodycamel::BlockingReaderWriterCircularBuffer<Lowl::AudioFrame>(10);

    initialized = true;
}

int LowlAudioStream::get_channels() const {
    return channels;
}

double LowlAudioStream::get_sample_rate() const {
    return sample_rate;
}

Lowl::SampleFormat LowlAudioStream::get_sample_format() const {
    return sample_format;
}

int LowlAudioStream::get_bytes_per_frame() const {
    return bytes_per_frame;
}

Lowl::AudioFrame LowlAudioStream::read(size_t &length) const {
    Lowl::AudioFrame frame;
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
        Lowl::AudioFrame frame;
        if (!buffer->try_enqueue(frame)) {
            return;
        }
    }


}




