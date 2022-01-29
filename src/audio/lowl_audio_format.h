#ifndef LOWL_AUDIO_FORMAT
#define LOWL_AUDIO_FORMAT

namespace Lowl::Audio {
    enum class AudioFormat {
        Unknown = 0,
        WAVE_FORMAT_PCM = 1,
        WAVE_FORMAT_IEEE_FLOAT = 2,
        WAVE_FORMAT_ALAW = 3,
        WAVE_FORMAT_MULAW = 4,
        WAVE_FORMAT_ADPCM = 5,
        WAVE_FORMAT_DVI_ADPCM = 6,
        MP3 = 7,
        FLAC = 8,
    };
}

#endif