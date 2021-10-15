#ifndef LOWL_DR_IMPL_H
#define LOWL_DR_IMPL_H

#include <dr_flac.h>
#include <dr_mp3.h>
#include <dr_wav.h>

drwav_uint64 lowl_drwav_size_max();

drwav_uint64 lowl_drflac_size_max();

drwav_bool32 lowl_drwav__is_compressed_format_tag(drwav_uint16 formatTag);

drwav_uint32 lowl_drwav_get_bytes_per_pcm_frame(drwav *pWav);

#endif /* LOWL_DR_IMPL_H */