#pragma once

#include "util/Assert.hpp"


// Thread pool specifically for slow and blocking tasks
// e.g. sync IO or blocking vulkan calls
class BlockingThreadPool
{
    using ToStartLot = OpParkingLot<>;

    using OpBase = ToStartLot::OpBase;

    template<class Receiver>
    struct Op : OpBase
    {
        Op(BlockingThreadPool& p, auto&& rec)
            : OpBase(this)
            , pool{p}
            , receiver{std::forward<decltype(rec)>(rec)}
        {
        }

        void start() noexcept
        {
            pool.enqueue(this);
        }
        
        void wake()
        {
            unifex::set_value(std::move(receiver));
        }

        void cancel()
        {
            unifex::set_done(std::move(receiver));
        }

        BlockingThreadPool& pool;
        Receiver receiver;
    };
public:
    class Scheduler
    {
        struct Sender
        {
            template <
                template <typename...> class Variant,
                template <typename...> class Tuple>
            using value_types = Variant<Tuple<>>;

            template <template <typename...> class Variant>
            using error_types = Variant<>;
            
            static constexpr bool sends_done = true;

            template<unifex::receiver_of<> Receiver>
            auto connect(Receiver&& r)
            {
                return Op<std::remove_cvref_t<Receiver>>{*pool, std::forward<Receiver>(r)};
            }

            BlockingThreadPool* pool;
        };
    public:
        explicit Scheduler(BlockingThreadPool* pool) : pool_{pool} {}

        Sender schedule() const
        {
            return Sender{pool_};
        }

        friend bool operator==(const Scheduler& a, const Scheduler& b) = default;

    private:
        BlockingThreadPool* pool_;
    };

    
    explicit BlockingThreadPool(std::size_t thread_count = std::thread::hardware_concurrency())
    {
        NG_ASSERT(thread_count > 0);
        threads_.reserve(thread_count);
        for (std::size_t i = 0; i < thread_count; ++i)
        {
            threads_.emplace_back([this]() { thread_loop(); });
        }
    }

    Scheduler get_scheduler() noexcept { return Scheduler{this}; }

    void request_stop() noexcept
    {
        stop_requested_.store(true, std::memory_order::relaxed);
        tasks_available_.notify_one();
    }

    ~BlockingThreadPool() noexcept
    {
        for (auto& thread : threads_)
        {
            thread.join();
        }
    }

private:
    void enqueue(OpBase* op)
    {
        std::unique_lock lock{mtx_};
        awaiting_start_.park(op);
        tasks_available_.notify_one();
    }

    void thread_loop()
    {
        while (!stop_requested_.load(std::memory_order::relaxed))
        {
            std::unique_lock lock{mtx_};
            while (!stop_requested_.load(std::memory_order::relaxed)
                && !awaiting_start_.wake_one(lock))
            {
                tasks_available_.wait(lock);
            }
        }

        std::unique_lock lock{mtx_};
        multi_cancel_all(lock, awaiting_start_);
    }
    
private:
    std::vector<std::thread> threads_;
    std::mutex mtx_;
    std::condition_variable tasks_available_;
    std::atomic<bool> stop_requested_{false};
    ToStartLot awaiting_start_;
};




