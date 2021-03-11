#ifndef LOWL_AUDIO_READER_H
#define LOWL_AUDIO_READER_H

#include "lowl_audio_stream.h"
#include "lowl_buffer.h"

#include <vector>

namespace Lowl {
    class AudioReader {
    public:
        static std::vector<AudioFrame> read_frames(SampleFormat format, Channel channel, void *data, size_t data_size);

    public:
        virtual std::unique_ptr<AudioStream>
        read_buffer(const std::unique_ptr<Buffer> &p_buffer, Error &error) = 0;

        virtual ~AudioReader() = default;

    public:
        std::unique_ptr<AudioStream> read_ptr(void *p_buffer, uint32_t p_length, Error &error);

        std::unique_ptr<AudioStream> read_file(const std::string &p_path, Error &error);
    };
}

#endif
