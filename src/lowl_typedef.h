#ifndef LOWL_TYPEDEF_H
#define LOWL_TYPEDEF_H

#include <cstdint>
#include <cstddef>

// Should always inline no matter what.
#ifndef _INLINE_
#if defined(__GNUC__)
#define _INLINE_ __attribute__((always_inline)) inline
#elif defined(_MSC_VER)
#define _INLINE_ __forceinline
#else
#define _INLINE_ inline
#endif
#endif

namespace Lowl {

    typedef std::uint16_t uint16_l;
    typedef std::uint32_t uint32_l;
    typedef std::size_t size_l;
    typedef double double_l;

    typedef uint16_l SpaceId;
    typedef double_l SampleRate;
}
#endif //LOWL_TYPEDEF_H
