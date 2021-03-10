#ifndef LOWL_AUDIO_READER_WAV_H
#define LOWL_AUDIO_READER_WAV_H

#include "lowl_audio_reader.h"

namespace Lowl {

    class AudioReaderWav : public AudioReader {

    protected:
        std::unique_ptr<AudioStream>
        read_buffer(const std::unique_ptr<Buffer> &p_buffer, Error &error) override;
    };
}

#endif
