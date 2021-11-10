#pragma once

#include <atomic>
#include <concepts>
#include <CachelinePad.hpp>


template<class T>
concept QueueNode = requires(T t) { { t.node } -> std::same_as<std::atomic<T*>> }; 

template<QueueNode Node>
class LockfreeQueue
{
public:
    void push(Node* node)
    {
        Node* last = last_.load();
        Node* next = nullptr;

        while (true)
        {
            next = last->next.load();
            if (next != nullptr)
            {
                last_.compare_exchange_weak(last, next);
                continue;
            }

            if (last->next.compare_exchange_weak(next, op))
            {
                break;
            }
        }

        // We succeeded and now last->next == op, so help other threads to advance the tail
        last_.compare_exchange_weak(last, op);
    }

    Node* pop()
    {
        Node* first = first_.load();
        Node* next = nullptr;

        do
        {
            next = first->next.load();
        }
        while (first_.compare_exchange_weak(first, next));

        return first;
    }

private:
    CachelinePad;
    std::atomic<Node*> first_;
    CachelinePad;
    std::atomic<Node*> last_;
};

namespace detail
{
    struct NodeTest { std::atomic<NodeTest*> next; };
    static_assert(sizeof(LockfreeQueue<NodeTest>) == CACHELINE_SIZE*2);
}

