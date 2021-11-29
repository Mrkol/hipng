#pragma once

#include <condition_variable>

#include "unifex/receiver_concepts.hpp"
#include "unifex/schedule_with_subscheduler.hpp"

#include "concurrency/OpParkingLot.hpp"
#include "concurrency/Spinlock.hpp"


// General purpose thread pool with subscheduler capabilities
class ThreadPool
{
    static constexpr std::size_t THREAD_NONE = ~std::size_t{0};

    using ToStartLot = OpParkingLot<>;

    using OpBase = ToStartLot::OpBase;

    template<class Receiver>
    struct Op : OpBase
    {
        Op(ThreadPool& p, auto&& rec, std::size_t tid)
            : OpBase(this)
            , pool{p}
            , receiver{std::forward<decltype(rec)>(rec)}
            , requested_thread{tid}
        {
        }

        void start() noexcept
        {
            pool.enqueue(this, requested_thread);
        }
        
        void wake()
        {
            unifex::set_value(std::move(receiver));
        }

        void cancel()
        {
            unifex::set_done(std::move(receiver));
        }

        ThreadPool& pool;
        Receiver receiver;
        std::size_t requested_thread;
    };

public:
    class Scheduler
    {
    public:
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
                return Op<std::remove_cvref_t<Receiver>>(*pool, std::forward<Receiver>(r), requested_thread);
            }

            std::size_t requested_thread;
            ThreadPool* pool;
        };
        
    public:
        explicit Scheduler(ThreadPool* pool) : pool_{pool} {}

        Sender schedule() const
        {
            return Sender{THREAD_NONE, pool_};
        }

        Sender schedule_with_subscheduler() const
        {
            return Sender{pool_->this_thread_idx_, pool_};
        }

        friend Sender tag_invoke(unifex::tag_t<unifex::schedule_with_subscheduler>, const Scheduler& scheduler)
        {
            return scheduler.schedule_with_subscheduler();
        }

        friend bool operator==(const Scheduler& a, const Scheduler& b) = default;

    private:
        ThreadPool* pool_;
    };


    explicit ThreadPool(std::size_t thread_count = std::thread::hardware_concurrency());

    Scheduler get_scheduler() noexcept { return Scheduler{this}; }

    void request_stop() noexcept;

    ~ThreadPool() noexcept;

private:
    static thread_local std::size_t this_thread_idx_;

    void enqueue(OpBase* op, std::size_t requested_thread);

    void thread_loop(std::size_t i);

    void run_task();

    using LockType = Spinlock;

    struct ThreadData
    {
        LockType spinlock;
        ToStartLot awaiting_start;
        
        LockType pinned_spinlock;
        ToStartLot awaiting_start_pinned;
        
        std::condition_variable_any any_awaiting;
    };



private:
    std::vector<std::thread> threads_;
    std::vector<ThreadData> thread_data_;
    std::atomic<std::size_t> current_thread_round_robin_{0};
    std::atomic<bool> stop_requested_{false};
};


