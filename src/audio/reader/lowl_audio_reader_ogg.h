#ifndef LOWL_AUDIO_READER_OGG_H
#define LOWL_AUDIO_READER_OGG_H

#include "lowl_audio_reader.h"

namespace Lowl::Audio {

    class AudioReaderOgg : public AudioReader {

    public:
        std::unique_ptr<AudioData> read(std::unique_ptr<uint8_t[]> p_buffer, size_t p_size, Error &error) override;

        bool support(FileFormat p_file_format) const override;
    };
}
#endif