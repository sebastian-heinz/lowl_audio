#ifndef LOWL_AUDIO_ASSET_HANDLE_H
#define LOWL_AUDIO_ASSET_HANDLE_H

#include "lowl_typedef.h"

namespace Lowl {
    struct AudioAssetHandle {
        AudioInstanceId owner_id = 0;
        AudioAssetId id = 0;
        AudioGeneration generation = 0;

        bool is_valid() const {
            return owner_id != 0 && id != 0 && generation != 0;
        }

        bool operator==(const AudioAssetHandle &p_other) const {
            return owner_id == p_other.owner_id && id == p_other.id && generation == p_other.generation;
        }

        bool operator!=(const AudioAssetHandle &p_other) const {
            return !(*this == p_other);
        }
    };

    static_assert(sizeof(AudioAssetHandle) == 12,
                  "AudioAssetHandle must remain a compact 32/16/32-bit value");
} // namespace Lowl

#endif // LOWL_AUDIO_ASSET_HANDLE_H
