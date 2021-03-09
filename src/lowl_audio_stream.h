#ifndef LOWL_AUDIO_STREAM_H
#define LOWL_AUDIO_STREAM_H

#include "lowl_sample_format.h"
#include "lowl_error.h"
#include "lowl_audio_frame.h"

#include <readerwritercircularbuffer.h>

namespace Lowl {

/**
 * Represents audio data.
 * - ability to write data
 * - current position / max
 * - data access for simultaneously read/write of buffer
 */
    class AudioStream {

    private:
        bool initialized;
        SampleFormat sample_format;
        double sample_rate;
        int channels;
        int sample_size;
        int bytes_per_frame;
        moodycamel::BlockingReaderWriterCircularBuffer<AudioFrame> *buffer;

    public:
        /***
         * call once all properties are set, before writing data
         */
        void initialize(SampleFormat p_sample_format, double p_sample_rate, int p_channels, LowlError &error);

        SampleFormat get_sample_format() const;

        double get_sample_rate() const;

        int get_channels() const;

        int get_bytes_per_frame() const;

        AudioFrame read(size_t &length) const;

        void write(void *data, size_t length);

        AudioStream();

        ~AudioStream();
    };
}


#endif