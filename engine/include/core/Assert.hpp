#pragma once

#include <string_view>
#include <source_location>


namespace detail
{

[[noreturn]] void panic(std::source_location loc, std::string_view message);

}

#define NG_PANIC(msg)                                          \
    do                                                         \
    {                                                          \
        detail::panic(std::source_location::current(), (msg)); \
    } while (false)

// TODO: logging, assert disabling, etc


// Fires in all builds
#define NG_VERIFY(cond, msg)                                \
    do                                                      \
    {                                                       \
        if (!(cond))                                        \
        {                                                   \
            NG_PANIC(msg);                                  \
        }                                                   \
    } while (false)

// Fires only in the debug build
#ifndef NDEBUG
#define NG_ASSERT(cond, msg) NG_VERIFY(cond, msg)
#else
#define NG_ASSERT(cond, msg)
#endif



