#pragma once

#include <unifex/detail/intrusive_queue.hpp>
#include <mutex>
#include <array>




namespace detail
{
    template<class T>
    concept HasCancel = requires(T t) { t.cancel(); };
}

template<class... WakeArgs>
class OpParkingLot
{
public:
    struct OpBase;

    using WakeFunc = void (*)(OpBase*, WakeArgs...);
    using CancelFunc = void (*)(OpBase*);

    struct OpBase
    {
        template<class Derived>
        explicit OpBase(Derived*)
            : wake_type_erased {+[](OpBase* op, WakeArgs... args) { static_cast<Derived*>(op)->wake(args...); }}
            , cancel_type_erased {
                    +[](OpBase* op)
                    {
                        if constexpr (detail::HasCancel<Derived>)
                        {
                           static_cast<Derived*>(op)->cancel();
                        }
                    }
                }
        {
        }

        void wake(WakeArgs... args)
        {
            wake_type_erased(this, args...);
        }

        void cancel()
        {
            cancel_type_erased(this);
        }

        OpBase* next{nullptr};
        WakeFunc wake_type_erased;
        CancelFunc cancel_type_erased;
    };

    void park(OpBase* op)
    {
        push(op);
    }

    template<class T>
    bool wake_one(std::unique_lock<T>& lock, WakeArgs... args)
    {
        if (auto op = pop())
        {
            lock.unlock();
            op->wake(args...);
            return true;
        }
        return false;
    }

    template<class T>
    void wake_all(std::unique_lock<T>& lock)
        requires (sizeof...(WakeArgs) == 0)
    {
        auto current = first_;
        first_ = nullptr;
        last_ = nullptr;
        lock.unlock();

        while (current != nullptr)
        {
            auto next = current->next;
            // wake might delete current
            current->wake();
            current = next;
        }
    }

    template<class T, class... Lots>
        requires (sizeof...(Lots) > 0)
    friend void multi_cancel_all(std::unique_lock<T>& lock, Lots&... lots);

private:
    OpBase* pop()
    {
        auto result = first_;
        if (result == nullptr)
        {
            return nullptr;
        }

        first_ = result->next;

        if (first_ == nullptr)
        {
            last_ = nullptr;
        }

        return result;
    }

    void push(OpBase* op)
    {
        if (first_ == nullptr)
        {
            first_ = op;
            last_ = op;
            return;
        }

        last_->next = op;
        last_ = op;
    }

private:
    OpBase* first_{nullptr};
    OpBase* last_{nullptr};
};


// Cancels several lots at a time
template<class T, class... Lots> // TODO: require Lots to be actual lots
    requires (sizeof...(Lots) > 0)
void multi_cancel_all(std::unique_lock<T>& lock, Lots&... lots)
{
    std::array currents{lots.first_...};

    (lots.first_ = ... = nullptr);
    (lots.last_ = ... =  nullptr);

    lock.unlock();

    for (auto current : currents)
    {
        while (current != nullptr)
        {
            auto next = current->next;
            // wake might delete current
            current->cancel();
            current = next;
        }   
    }
}




