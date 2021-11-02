#pragma once

#include <string_view>
#include <spdlog/spdlog.h>


// TODO: remove when clang stops being a piece of shit
#if defined(__clang__)

namespace detail
{

struct SourceLocation
{
    [[nodiscard]] std::string_view file_name() const { return file_; }
    [[nodiscard]] std::uint32_t line() const { return line_; }
    [[nodiscard]] std::string_view function_name() const { return func_; }

    std::string_view file_;
    std::uint32_t line_;
    std::string_view func_;
};

}

#define NG_CURRENT_LOCATION detail::SourceLocation{__FILE__, __LINE__. __PRETTY_FUNCTION__}

#else
#include <source_location>
namespace detail
{

using SourceLocation = std::source_location;

}
#define NG_CURRENT_LOCATION std::source_location::current()
#endif


namespace detail
{

template<typename... Ts>
[[noreturn]] void panic(SourceLocation loc, std::string_view message, Ts&&... ts)
{
    spdlog::critical("Panicked at {}:{} ({})", loc.file_name(), loc.line(), loc.function_name());
    spdlog::critical(message, ts...);
    std::abort();
}

}

#define NG_PANIC(msg, ...)                                                   \
    do                                                                       \
    {                                                                        \
        detail::panic(NG_CURRENT_LOCATION, (msg), ##__VA_ARGS__); \
    } while (false)

// TODO: logging, assert disabling, etc


// Fires in all builds
#define NG_VERIFY(cond, msg, ...)                           \
    do                                                      \
    {                                                       \
        if (!(cond))                                        \
        {                                                   \
            NG_PANIC(msg, ##__VA_ARGS__);        \
        }                                                   \
    } while (false)

// Fires only in the debug build
#ifndef NDEBUG
#define NG_ASSERT(cond, msg) ((void) (cond));
#else
#define NG_ASSERT(cond, msg, ...) NG_VERIFY(cond, msg, ##__VA_ARGS__)
#endif
