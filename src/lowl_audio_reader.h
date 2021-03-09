#ifndef LOWL_AUDIO_READER_H
#define LOWL_AUDIO_READER_H

#include <string>
#include <memory>

#include "lowl_buffer.h"
#include "lowl_file.h"
#include "lowl_audio_stream.h"
#include "lowl_error.h"

namespace Lowl {
    class AudioReader {
    public:
        virtual std::unique_ptr<LowlAudioStream>
        read_buffer(const std::unique_ptr<Buffer> &p_buffer, LowlError &error) = 0;

        std::unique_ptr<LowlAudioStream> read_ptr(void *p_buffer, uint32_t p_length, LowlError &error);

        std::unique_ptr<LowlAudioStream> read_file(const std::string &p_path, LowlError &error);

        virtual ~AudioReader() = default;
    };
}

#endif
