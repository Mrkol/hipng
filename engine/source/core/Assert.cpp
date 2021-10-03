#include "core/Assert.hpp"

#include <iostream>
#include <mutex>

#include "spdlog/spdlog.h"


static std::mutex mutex;

void detail::panic(std::source_location loc, std::string_view message)
{
    {
        // TODO: figure out spdlog multithreading
        std::lock_guard lock{mutex};
        spdlog::critical("Panicked at {}:{} ({}) with error\n{}", loc.file_name(), loc.line(), loc.function_name(), message);
    }
    std::abort();
}
