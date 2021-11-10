#pragma once


constexpr std::size_t CACHELINE_SIZE = 64;

[[no_unique_address]] struct alignas(CACHELINE_SIZE) CachelinePad {};
