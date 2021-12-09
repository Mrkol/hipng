#pragma once

#include <array>
#include <type_traits>
#include <unifex/sender_concepts.hpp>


#include "util/Assert.hpp"
#include "concurrency/Spinlock.hpp"
#include "concurrency/OpParkingLot.hpp"


template<std::size_t N, unifex::sender Sender>
class StaticScope
{
    struct DoneReceiver
    {
        void set_value() &&
        {
            std::move(*this).set_done();
        }

        [[noreturn]] void set_error(auto err) noexcept
        {
            if constexpr (std::same_as<decltype(err), std::exception_ptr>)
            {
                try
                {
			        std::rethrow_exception(err);
			    }
            	catch (const std::exception& e)
                {
			        spdlog::error(e.what());
			    }
                catch(...)
                {
	                spdlog::error("Something different from an exception got thrown in the rendering scope!");
                }
            }
            std::terminate();
        }

        void set_done() && noexcept
        {
            scope->on_done(slot);
        }
        
        StaticScope* scope;
        std::size_t slot;
    };

    using SpawnLot = OpParkingLot<std::size_t>;
    using SpawnOpBase = SpawnLot::OpBase;

    template<class Receiver>
    struct SpawnOp : SpawnOpBase
    {
        template<class Receiver2>
        SpawnOp(StaticScope& s, Receiver2&& r, Sender&& sender)
            : SpawnOpBase(this)
            , scope{s}
            , on_spawned_receiver{std::forward<Receiver2>(r)}
            , sender_to_spawn{std::move(sender)}
        {
        }

        void wake(std::size_t slot)
        {
            scope.storage_[slot].op.construct_with([&]()
                {
                    return unifex::connect(std::move(sender_to_spawn), DoneReceiver{&scope, slot});
                });

            unifex::start(scope.storage_[slot].op.get());

            std::move(on_spawned_receiver).set_value();
        }

        void start() noexcept
        {
            scope.do_spawn(this);
        }

        StaticScope& scope;
        Receiver on_spawned_receiver;
        Sender sender_to_spawn;
    };
    
    struct SpawnedSender
    {
        template <
            template <typename...> class Variant,
            template <typename...> class Tuple>
        using value_types = Variant<Tuple<>>;

        template <template <typename...> class Variant>
        using error_types = Variant<>;
        
        static constexpr bool sends_done = false;


        template<class Sender2>
        SpawnedSender(StaticScope* parent, Sender2&& sender)
            : parent_scope{parent}, sender_to_spawn{std::forward<Sender2>(sender)}
        {}

        template<unifex::receiver_of<> Receiver>
        auto connect(Receiver&& receiver)
        {
            return SpawnOp<std::remove_cvref_t<Receiver>>
                {*parent_scope, std::forward<Receiver>(receiver), std::move(sender_to_spawn)};
        }

        StaticScope* parent_scope;
        Sender sender_to_spawn;
    };

    using AllFinishedLot = OpParkingLot<>;
    using AllFinishedOpBase = AllFinishedLot::OpBase;

    template<class Receiver>
    struct AllFinishedOp : AllFinishedOpBase
    {
        template<class Receiver2>
        AllFinishedOp(StaticScope& s, Receiver2&& r)
            : AllFinishedOpBase(this)
            , scope{s}
            , receiver{r}
        {
        }

        void start() noexcept
        {
            scope.do_wait_all_done(this);
        }

        void wake()
        {
            std::move(receiver).set_value();
        }

        StaticScope& scope;
        Receiver receiver;
    };

    struct AllFinishedSender
    {
        template <
            template <typename...> class Variant,
            template <typename...> class Tuple>
        using value_types = Variant<Tuple<>>;

        template <template <typename...> class Variant>
        using error_types = Variant<>;
        
        static constexpr bool sends_done = false;

        template<class Receiver>
        friend auto tag_invoke(unifex::tag_t<unifex::connect>, AllFinishedSender sender, Receiver&& receiver)
        {
            return AllFinishedOp<std::remove_cvref_t<Receiver>>
                {*sender.parent_scope, std::forward<Receiver>(receiver)};
        }

        StaticScope* parent_scope;
    };
public:
    StaticScope(std::size_t size) noexcept
        : size_{0}
        , capacity_{size}
    {
        NG_ASSERT(size <= N);
        for (std::size_t i = 0; i < storage_.size(); ++i)
        {
            storage_[i].next_free = i+1;
        }
    }

    template<std::convertible_to<Sender> Sender2>
    SpawnedSender spawn_next(Sender2&& sender)
    {
        return SpawnedSender{this, std::forward<Sender2>(sender)};
    }

    AllFinishedSender all_finished() const
    {
        return AllFinishedSender{this};
    }
    
private:
    void free_slot(std::size_t slot)
    {
        // clear op preemptively to free resources
        storage_[slot].op.destruct();
        storage_[slot].next_free = first_free_slot_;
        first_free_slot_ = slot;
        --size_;
    }

    void on_done(std::size_t slot) noexcept
    {
        std::unique_lock lock{spinlock_};

        // Try and wake someone straight into this slot. If unsuccessful,
        // free the slot
        if (!awaiting_spawn_.wake_one(lock, slot))
        {
            free_slot(slot);
            
            if (size_ == 0)
            {
                awaiting_all_finished_.wake_all(lock);
            }
        }
    }

    std::size_t take_slot()
    {
        ++size_;
        auto slot = first_free_slot_;
        first_free_slot_ = storage_[slot].next_free;
        return slot;
    }

    void do_spawn(SpawnOpBase* op)
    {
        std::unique_lock lock{spinlock_};

        if (size_ < capacity_)
        {
            auto slot = take_slot();
            lock.unlock();

            // Unlocking before waking is important!
            // Wake starts another operation that might use this static scope again and deadlock
            op->wake(slot);

            return;
        }

        awaiting_spawn_.park(op);
    }

    void do_wait_all_done(AllFinishedOpBase* op)
    {
        std::unique_lock guard{spinlock_};
        if (size_ > 0)
        {
            awaiting_all_finished_.park(op);
            return;
        }


        guard.unlock();
        op->wake();
    }
    
private:
    struct Slot
    {
        unifex::manual_lifetime<unifex::connect_result_t<Sender, DoneReceiver>> op;
        std::size_t next_free;
    };

    // Storage is actually a stack of slots (the +1 is for the sentinel)
    // first_free_slot_ is the head, size_ is the size, duh
    std::array<Slot, N + 1> storage_;
    std::size_t first_free_slot_{0};
    std::size_t size_;
    std::size_t capacity_;
    
    SpawnLot awaiting_spawn_;
    AllFinishedLot awaiting_all_finished_;
    
    // Guards everything. Do be careful with this one, as the logic behind unlocking and
    // starting new ops is tricky
    Spinlock spinlock_;
};
