#pragma once

#include <type_traits>

/*
 * Macro to allow enum values to be combined and evaluated as flags.
 *  * Based on:
 *  - DEFINE_ENUM_FLAG_OPERATORS from <winnt.h>
 *  - https://stackoverflow.com/a/63031334/1624459
 */
#define NG_MAKE_ENUM_FLAGS(TEnum)                                                   \
    inline constexpr TEnum operator|(TEnum a, TEnum b)                              \
	{                                                                               \
        using TUnder = typename std::underlying_type_t<TEnum>;                      \
        return static_cast<TEnum>(static_cast<TUnder>(a) | static_cast<TUnder>(b)); \
    }                                                                               \
    inline constexpr TEnum operator&(TEnum a, TEnum b)                              \
	{                                                                               \
        using TUnder = typename std::underlying_type_t<TEnum>;                      \
        return static_cast<TEnum>(static_cast<TUnder>(a) & static_cast<TUnder>(b)); \
    }                                                                               \
    inline constexpr TEnum& operator|=(TEnum& a, TEnum b)                           \
	{                                                                               \
        using TUnder = typename std::underlying_type_t<TEnum>;                      \
        a = static_cast<TEnum>(static_cast<TUnder>(a) | static_cast<TUnder>(b));    \
        return a;                                                                   \
    }                                                                               \
    inline constexpr TEnum& operator&=(TEnum& a, TEnum b)                           \
	{                                                                               \
        using TUnder = typename std::underlying_type_t<TEnum>;                      \
        a = static_cast<TEnum>(static_cast<TUnder>(a) & static_cast<TUnder>(b));    \
        return a;                                                                   \
    }                                                                               \
