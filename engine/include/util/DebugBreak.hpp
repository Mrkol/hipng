#pragma once

#include "util/TargetInfo.hpp"
#include <csignal>


#ifdef NG_COMPILER_MSVC
#   define DEBUG_BREAK() do { __debugbreak(); } while (false)
#elif defined(SIGTRAP)
#   define DEBUG_BREAK() do { raise(SIGTRAP); } while (false)
#endif
