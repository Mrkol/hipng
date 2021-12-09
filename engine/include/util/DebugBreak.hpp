#pragma once

#include "util/TargetInfo.hpp"


#ifdef NG_COMPILER_MSVC
#   define DEBUG_BREAK() do { __debugbreak(); } while (false)
#elif __has_include(<signal.h>) && defined(SIGTRAP)
#	define DEBUG_BREAK() do { raise(SIGTRAP); } while (false)
#endif
