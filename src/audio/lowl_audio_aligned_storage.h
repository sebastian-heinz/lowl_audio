#ifndef LOWL_AUDIO_ALIGNED_STORAGE_H
#define LOWL_AUDIO_ALIGNED_STORAGE_H

#include <cstdlib>
#include <cstring>
#include <memory>

#ifdef _MSC_VER
#include <malloc.h>
#endif

#include "lowl_typedef.h"

namespace Lowl::Audio {
    static constexpr size_t kSampleStorageAlignmentBytes = 64;

    struct SampleStorageDeleter {
        void operator()(Sample *p_ptr) const noexcept {
            if (p_ptr == nullptr) {
                return;
            }
#ifdef _MSC_VER
            _aligned_free(p_ptr);
#else
            std::free(p_ptr);
#endif
        }
    };

    using SampleStoragePtr = std::unique_ptr<Sample, SampleStorageDeleter>;

    LOWL_INLINE size_t aligned_frame_stride(const size_t p_frame_count) {
        const size_t samples_per_alignment =
            std::max<size_t>(1, kSampleStorageAlignmentBytes / sizeof(Sample));
        const size_t remainder = p_frame_count % samples_per_alignment;
        return remainder == 0 ? p_frame_count : p_frame_count + (samples_per_alignment - remainder);
    }

    inline SampleStoragePtr allocate_aligned_samples(const size_t p_sample_count) {
        if (p_sample_count == 0) {
            return SampleStoragePtr{};
        }

        void *storage = nullptr;
#ifdef _MSC_VER
        storage = _aligned_malloc(p_sample_count * sizeof(Sample), kSampleStorageAlignmentBytes);
        if (storage == nullptr) {
            throw std::bad_alloc();
        }
#else
        if (posix_memalign(&storage, kSampleStorageAlignmentBytes, p_sample_count * sizeof(Sample)) != 0) {
            throw std::bad_alloc();
        }
#endif

        std::memset(storage, 0, p_sample_count * sizeof(Sample));
        return SampleStoragePtr(static_cast<Sample *>(storage));
    }
} // namespace Lowl::Audio

#endif
