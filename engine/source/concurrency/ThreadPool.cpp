#include "concurrency/ThreadPool.hpp"

#include "util/Assert.hpp"
#include "concurrency/MultiLock.hpp"


thread_local std::size_t ThreadPool::this_thread_idx_ = THREAD_NONE;


ThreadPool::ThreadPool(std::size_t thread_count): thread_data_{thread_count}
{
    NG_ASSERT(thread_count > 0);
    threads_.reserve(thread_count);
    for (std::size_t i = 0; i < thread_count; ++i)
    {
        threads_.emplace_back([i, this]() { thread_loop(i); });
    }
}

void ThreadPool::request_stop() noexcept
{
    stop_requested_.store(true, std::memory_order::relaxed);
    for (auto& data : thread_data_)
    {
        data.any_awaiting.notify_one();
    }
}

void ThreadPool::enqueue(OpBase* op, std::size_t requested_thread)
{
    if (requested_thread != THREAD_NONE)
    {
        auto& data = thread_data_[requested_thread];
        std::lock_guard lock{data.pinned_spinlock};
        data.awaiting_start_pinned.park(op);
        data.any_awaiting.notify_one();
        return;
    }

    auto target_queue = current_thread_round_robin_.fetch_add(1, std::memory_order::relaxed) % threads_.size();

    for (std::size_t i = 0; i < threads_.size(); ++i)
    {
        auto j = i + target_queue;
        if (j >= threads_.size()) { j -= threads_.size(); }

        auto& data = thread_data_[j];
        if (std::unique_lock lock{data.spinlock, std::try_to_lock})
        {
            data.awaiting_start.park(op);
            data.any_awaiting.notify_one();
            return;
        }
    }

    auto& data = thread_data_[target_queue];
    std::lock_guard lock{data.spinlock};
    data.awaiting_start.park(op);
    data.any_awaiting.notify_one();
}

void ThreadPool::thread_loop(std::size_t tid)
{
    this_thread_idx_ = tid;


    while (!stop_requested_.load(std::memory_order::relaxed))
    {
        run_task();
    }


    auto& this_thread_data = thread_data_[this_thread_idx_];
    std::unique_lock lock{this_thread_data.spinlock};
    multi_cancel_all(lock, this_thread_data.awaiting_start, this_thread_data.awaiting_start_pinned);
}

void ThreadPool::run_task()
{
    auto& this_thread_data = thread_data_[this_thread_idx_];

    if (std::unique_lock lock{this_thread_data.pinned_spinlock, std::try_to_lock};
        lock && this_thread_data.awaiting_start_pinned.wake_one(lock))
    {
        return;
    }


    for (std::size_t i = 0; i < threads_.size(); ++i)
    {
        auto j = i + this_thread_idx_;
        if (j >= threads_.size()) { j -= threads_.size(); }
        auto& queue_data = thread_data_[j];
        if (std::unique_lock lock{queue_data.spinlock, std::try_to_lock};
            lock && queue_data.awaiting_start.wake_one(lock))
        {
            return;
        }
    }


    MultiLock multilock(this_thread_data.pinned_spinlock, this_thread_data.spinlock);
    std::unique_lock lock{multilock};
    while (!stop_requested_.load(std::memory_order::relaxed)
        && !this_thread_data.awaiting_start_pinned.wake_one(lock)
        && !this_thread_data.awaiting_start.wake_one(lock))
    {
        this_thread_data.any_awaiting.wait(lock);
    }
}

ThreadPool::~ThreadPool() noexcept
{
    for (auto& thread : threads_)
    {
        thread.join();
    }
}
