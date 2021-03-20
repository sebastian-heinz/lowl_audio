#ifndef LOWL_TYPEDEF_H
#define LOWL_TYPEDEF_H

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

#endif //LOWL_TYPEDEF_H
