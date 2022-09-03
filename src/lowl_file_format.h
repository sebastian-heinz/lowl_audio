#ifndef LOWL_FILE_FORMAT
#define LOWL_FILE_FORMAT

namespace Lowl {
    enum class FileFormat {
        UNKNOWN = 0,
        WAV = 1,
        MP3 = 2,
        FLAC = 3,
        OGG = 4,
        OPUS = 5,
    };
}

#endif