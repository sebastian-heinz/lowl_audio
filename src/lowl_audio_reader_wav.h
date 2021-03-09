#ifndef LOWL_AUDIO_READER_WAV_H
#define LOWL_AUDIO_READER_WAV_H

#include "../include/lowl_audio_reader.h"

class LowlAudioReaderWav : public LowlAudioReader {

protected:
    std::unique_ptr<LowlAudioStream>
    read_buffer(const std::unique_ptr<LowlBuffer> &p_buffer, LowlError &error) override;
};

#endif
