#pragma once


#if defined(__clang__)
#   define NG_COMPILER_CLANG
#elif defined(__GNUG__)
#   define NG_COMPILER_GCC
#elif defined(_MSC_VER)
#   define NG_COMPILER_MSVC
#else
#   error Unsupported compiler!
#endif

#if defined(NG_COMPILER_GCC) || defined(NG_COMPILER_CLANG)
#   if defined(__amd64__)
#       define NG_ARCH_x86
#   elif defined(__arm__)
#       define NG_ARCHITECTURE_ARM
#   else
#       error Unsupported architecture!
#   endif
#elif defined(NG_COMPILER_MSVC)
#   if defined(_M_AMD64)
#       define NG_ARCH_x86
#   elif defined(_M_ARM)
#       define NG_ARCH_ARM
#   else
#       error Unsupported architecture!
#   endif
#endif
