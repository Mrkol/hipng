#pragma once

#include "util/TargetInfo.hpp"


#if defined(NG_ARCH_x86) && __has_include(<immintrin.h>)
#   include <immintrin.h>
#   define NG_USE_INTEL_INTRINSIC
#endif

#if defined(NG_ARCH_ARM) && __has_include(<arm_neon.h>)
#   include <arm_neon.h>
#   define NG_USE_ARM_INTRINSIC
#endif

inline void cpu_pause()
{
#if defined(NG_USE_INTEL_INTRINSIC)
    _mm_pause();
#   undef NG_USE_INTEL_INTRINSIC
#elif defined(NG_USE_ARM_INTRINSIC)
    __yield();
#   undef NG_USE_ARM_INTRINSIC
#else
    // TODO: some sensible default?
#endif
}


class SpinWait
{
public:
    constexpr static std::size_t YIELD_THRESHOLD = 20;

    void operator()()
    {
        if (counter_ < YIELD_THRESHOLD)
        {
            cpu_pause();
        }
        else
        {
            std::this_thread::yield();
        }
        ++counter_;
    }

private:
    std::size_t counter_{0};
};
