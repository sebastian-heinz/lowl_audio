#ifndef LOWL_TYPEDEF_H
#define LOWL_TYPEDEF_H

#include <cstdlib>

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

    typedef u_int16_t SpaceId;
    typedef double SampleRate;

    typedef u_int16_t uint16_l;
    typedef u_int32_t uint32_l;
    typedef size_t size_l;

}
#endif //LOWL_TYPEDEF_H
