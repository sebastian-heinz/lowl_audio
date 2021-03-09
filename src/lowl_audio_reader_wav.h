#ifndef LOWL_AUDIO_READER_WAV_H
#define LOWL_AUDIO_READER_WAV_H

#include "lowl_audio_reader.h"
#include "lowl_buffer.h"
#include "lowl_error.h"

namespace Lowl {

    class AudioReaderWav : public Lowl::AudioReader {

    protected:
        std::unique_ptr<LowlAudioStream>
        read_buffer(const std::unique_ptr<Buffer> &p_buffer, LowlError &error) override;
    };
}

#endif
