#ifndef LOWL_AUDIO_STREAM_H
#define LOWL_AUDIO_STREAM_H

#include <cstdint>
#include <memory>

#include "lowl_sample_format.h"
#include "lowl_buffer.h"
#include "lowl_error.h"
#include "lowl_audio_frame.h"

#include <readerwritercircularbuffer.h>

/**
 * Represents audio data.
 * - ability to write data
 * - current position / max
 * - data access for simultaneously read/write of buffer
 */
class LowlAudioStream {

private:
    bool initialized;
    Lowl::SampleFormat sample_format;
    double sample_rate;
    int channels;
    int sample_size;
    int bytes_per_frame;
    moodycamel::BlockingReaderWriterCircularBuffer<Lowl::AudioFrame> *buffer;

    inline int get_sample_size(Lowl::SampleFormat format);

public:
    /***
     * call once all properties are set, before writing data
     */
    void initialize(Lowl::SampleFormat p_sample_format, double p_sample_rate, int p_channels, Lowl::LowlError &error);

    Lowl::SampleFormat get_sample_format() const;

    double get_sample_rate() const;

    int get_channels() const;

    int get_bytes_per_frame() const;

    Lowl::AudioFrame read(size_t &length) const;

    void write(void *data, size_t length);

    LowlAudioStream();

    ~LowlAudioStream();
};

#endif 