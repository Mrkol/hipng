#pragma once

#include <atomic>
#include <utility>
#include <array>


// TODO: move this somewhere
namespace detail
{
    template<class Sequence, class Aggregated = std::index_sequence<>, std::size_t Next = 0>
    struct PrefixSumHelper;

    template<class Aggregated, std::size_t Next>
    struct PrefixSumHelper<std::index_sequence<>, Aggregated, Next>
    {
        using Type = Aggregated;
    };

    template<std::size_t I, std::size_t... Is, std::size_t... Js, std::size_t J>
    struct PrefixSumHelper<std::index_sequence<I, Is...>, std::index_sequence<Js...>, J>
    {
        using Type = typename PrefixSumHelper<std::index_sequence<Is...>, std::index_sequence<Js..., J>, J + I>::Type;
    };

    template<std::size_t I, class T>
    using Second = T;
}


template<std::size_t... Sizes>
    requires ((Sizes + ...) <= 64)
class AtomicUIntTuple
{
    using PrefixSums = typename detail::PrefixSumHelper<std::index_sequence<Sizes...>>::Type;

public:
    using ValueType = std::array<std::uint64_t, sizeof...(Sizes)>;

    explicit AtomicUIntTuple(ValueType value) : impl_{encode(value)} {}

    ValueType load(std::memory_order m = std::memory_order::seq_cst)
        { return decode(impl_.load(m)); }
    void store(ValueType value, std::memory_order m = std::memory_order::seq_cst)
        { impl_.store(encode(value), m); }
    ValueType exchange(ValueType value, std::memory_order m = std::memory_order::seq_cst)
        { return decode(impl_.exchange(encode(value), m)); }

    template<bool Weak = false>
    void compare_exchange(ValueType& expected, ValueType desired,
        std::memory_order success = std::memory_order::seq_cst,
        std::memory_order failure = std::memory_order::seq_cst)
    {
        std::uint64_t old = encode(expected);

        if constexpr (Weak)
        { impl_.compare_exchange_weak(old, encode(desired), success, failure); }
        else
        { impl_.compare_exchange_strong(old, encode(desired), success, failure); }
        
        expected = decode(old);
    }

private:
    std::uint64_t encode(ValueType value)
    {
        return
            [&]
            <std::size_t... Idxs, std::size_t... Szs, std::size_t... Offsets>
            (std::index_sequence<Idxs...>, std::index_sequence<Szs...>, std::index_sequence<Offsets...>)
            {
                return (((std::get<Idxs>(value) & ((1 << Szs) - 1)) << Offsets) | ...);
            }
            (std::make_index_sequence<sizeof...(Sizes)>{}, std::index_sequence<Sizes...>{}, PrefixSums{});
    }

    ValueType decode(std::uint64_t a)
    {
        ValueType result;
        [&]
        <std::size_t... Idxs, std::size_t... Szs, std::size_t... Offsets>
        (std::index_sequence<Idxs...>, std::index_sequence<Szs...>, std::index_sequence<Offsets...>)
        {
            (..., (std::get<Idxs>(result) = (a >> Offsets) & ((1 << Szs) - 1)));
        }
        (std::make_index_sequence<sizeof...(Sizes)>{}, std::index_sequence<Sizes...>{}, PrefixSums{});
        return result;
    }

private:
    std::atomic<std::uint64_t> impl_;
};


