#define DR_FLAC_IMPLEMENTATION
#define DR_FLAC_NO_STDIO
#define DR_FLAC_NO_CRC

#define DR_MP3_IMPLEMENTATION
#define DR_MP3_FLOAT_OUTPUT
#define DR_MP3_NO_STDIO

#define DR_WAV_IMPLEMENTATION
#define DR_WAV_NO_STDIO

#include "../include/lowl_dr_imp.h"

DRWAV_INLINE drwav_uint64 lowl_drwav_size_max() {
    return DRWAV_SIZE_MAX;
}

DRWAV_INLINE drwav_uint64 lowl_drflac_size_max() {
    return DRFLAC_SIZE_MAX;
}

DRWAV_INLINE drwav_bool32 lowl_drwav__is_compressed_format_tag(drwav_uint16 formatTag) {
    return drwav__is_compressed_format_tag(formatTag);
}

drwav_uint32 lowl_drwav_get_bytes_per_pcm_frame(drwav *pWav) {
    return drwav_get_bytes_per_pcm_frame(pWav);
}

#undef DR_FLAC_IMPLEMENTATION
#undef DR_MP3_IMPLEMENTATION
#undef DR_MP3_IMPLEMENTATION