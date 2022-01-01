#pragma once


#include "concurrency/Spinlock.hpp"
#include "concurrency/OpParkingLot.hpp"
#include <mutex>


class EventQueue
{
    using EventLot = OpParkingLot<>;

    using OpBase = EventLot::OpBase;

    template<class Receiver>
    struct Op : OpBase
    {
        Op(EventQueue& q, auto&& rec)
            : OpBase(this)
            , queue{q}
            , receiver{std::forward<decltype(rec)>(rec)}
        {
        }

        void start() noexcept
        {
            queue.enqueue(this);
        }
        
        void wake()
        {
            unifex::set_value(std::move(receiver));
        }

        void cancel()
        {
            unifex::set_done(std::move(receiver));
        }

        EventQueue& queue;
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
                return Op<std::remove_cvref_t<Receiver>>{*queue, std::forward<Receiver>(r)};
            }

            EventQueue* queue;
        };
    public:
        explicit Scheduler(EventQueue* queue) : queue_{queue} {}

        Sender schedule() const
        {
            return Sender{queue_};
        }

        friend bool operator==(const Scheduler& a, const Scheduler& b) = default;

    private:
        EventQueue* queue_;
    };

    Scheduler get_scheduler() noexcept
    {
	    return Scheduler(this);
    }

    void executeAll()
    {
        std::unique_lock lock{lock_};
        events_.wake_all(lock);
    }

    ~EventQueue() noexcept
    {
        std::unique_lock lock{lock_};
        multi_cancel_all(lock, events_);
    }

private:
    void enqueue(OpBase* op)
    {
        std::unique_lock lock{lock_};
        events_.park(op);
    }


private:
    Spinlock lock_;
    EventLot events_;
};
