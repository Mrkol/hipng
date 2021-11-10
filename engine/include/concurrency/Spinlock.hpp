#pragma once

#include <atomic>
#include <thread>

#include "concurrency/SpinWait.hpp"


class Spinlock
{
public:
    void lock()
    {
        SpinWait wait;
        while (locked_.exchange(true, std::memory_order_acquire))
        {
            while (locked_.load(std::memory_order_relaxed))
            {
                wait();
            }
        }
    }

    bool try_lock()
    {
        return !locked_.exchange(true, std::memory_order_acquire);
    }

    void unlock()
    {
        locked_.store(false, std::memory_order_release);
    }

private:
    std::atomic<bool> locked_{false};
};
