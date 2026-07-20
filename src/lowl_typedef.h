#ifndef LOWL_TYPEDEF_H
#define LOWL_TYPEDEF_H

#include <atomic>
#include <cstddef>
#include <cstdint>

// Should always inline no matter what.
#ifndef LOWL_INLINE
#if defined(__GNUC__)
#define LOWL_INLINE __attribute__((always_inline)) inline
#elif defined(_MSC_VER)
#define LOWL_INLINE __forceinline
#else
#define LOWL_INLINE inline
#endif
#endif

#ifndef LOWL_RESTRICT
#if defined(__GNUC__) || defined(__clang__)
#define LOWL_RESTRICT __restrict__
#elif defined(_MSC_VER)
#define LOWL_RESTRICT __restrict
#else
#define LOWL_RESTRICT
#endif
#endif

namespace Lowl {
    typedef std::uint16_t uint16_l;
    typedef std::uint32_t uint32_l;
    typedef std::size_t size_l;
    typedef double double_l;
    typedef float float_l;

#ifdef LOWL_TYPE_SAMPLE_64
    typedef double_l Sample;
#else
    typedef float_l Sample;
#endif /* LOWL_TYPE_SAMPLE_64 */

    static_assert(std::atomic<Sample>::is_always_lock_free,
                  "std::atomic<Sample> must be lock-free for real-time audio safety");

    typedef uint16_l AudioAssetId;
    typedef uint16_l AudioPlaybackId;
    typedef double_l TimeSeconds;
    typedef uint32_l SampleCount;

    typedef double_l SampleRate;
    static constexpr SampleRate NO_SAMPLE_RATE = 0;

    typedef Sample Volume;
    static constexpr Volume DEFAULT_VOLUME = 1.0;

    /// Range from -1 to 1
    typedef Sample Panning;
    static constexpr Panning DEFAULT_PANNING = 0.0;
    static constexpr Panning MAX_PANNING = 1.0;
    static constexpr Panning MIN_PANNING = -1.0;
} // namespace Lowl
#endif // LOWL_TYPEDEF_H
