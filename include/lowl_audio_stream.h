#ifndef LOWL_AUDIO_STREAM_H
#define LOWL_AUDIO_STREAM_H

#include <cstdint>
#include <memory>

#include "../src/lowl_sample_format.h"
#include "../src/lowl_buffer.h"
#include "lowl_error.h"

#include <readerwritercircularbuffer.h>

/**
 * Represents audio data.
 * - ability to write data
 * - current position / max
 * - stream mode
 * - data access for simultaneously read/write of buffer
 */
class LowlAudioStream {

public:
    struct Frame {
        float left;
        float right;
    };


private:
    bool initialized;
    LowlSampleFormat sample_format;
    double sample_rate;
    int channels;
    int sample_size;
    int bytes_per_frame;
    moodycamel::BlockingReaderWriterCircularBuffer<Frame> *buffer;

    inline int get_sample_size(LowlSampleFormat format);

public:
    /***
     * call once all properties are set, before writing data
     */
    void initialize(LowlSampleFormat p_sample_format, double p_sample_rate, int p_channels, LowlError &error);

    LowlSampleFormat get_sample_format() const;

    double get_sample_rate() const;

    int get_channels() const;

    int get_bytes_per_frame() const;

    Frame read(size_t &length) const;

    void write(void *data, size_t length);

    LowlAudioStream();

    ~LowlAudioStream();
};

#endif 