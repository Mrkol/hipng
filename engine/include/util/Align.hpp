#pragma once

#include <concepts>


template<std::integral T>
T align(T v, T a)
{
    return (v / a + !!(v % a)) * a;
}
