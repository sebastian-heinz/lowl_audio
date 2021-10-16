#ifndef LOWL_DR_IMPL_H
#define LOWL_DR_IMPL_H

#include <dr_flac.h>
#include <dr_mp3.h>
#include <dr_wav.h>

namespace LowlThirdParty {
    class DrLib {
    public:
        static drwav_uint64 lowl_drwav_size_max();

        static drwav_bool32 lowl_drwav__is_compressed_format_tag(drwav_uint16 formatTag);

        static drwav_uint32 lowl_drwav_get_bytes_per_pcm_frame(drwav *pWav);
    };
}

#endif /* LOWL_DR_IMPL_H */