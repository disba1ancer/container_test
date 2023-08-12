#ifndef OC_QUEUE_H
#define OC_QUEUE_H

#include <atomic>

namespace container_test {

struct OCQueueNode {
    std::atomic<OCQueueNode*> next;
};

// One consumer multiple producer queue
struct OCQueue {
    OCQueue() noexcept :
        sentinel{{&sentinel}},
        tail(&sentinel)
    {}
    OCQueue(OCQueue&& oth) noexcept :
        sentinel{{oth.sentinel.next.load(std::memory_order_relaxed)}},
        tail([this, &oth]
        {
            auto last = oth.tail.load(std::memory_order_relaxed);
            last->next.store(&sentinel);
            return last;
        }())
    {}
    void Push(OCQueueNode* newNode)
    {
        newNode->next.store(&sentinel, std::memory_order_relaxed);
        auto pPtr = newNode;
        tail.exchange(pPtr, std::memory_order_release);
        auto end = &sentinel;
        if (!sentinel.next.compare_exchange_strong(
            end, newNode, std::memory_order_relaxed, std::memory_order_relaxed
        )) {
            pPtr->next.store(newNode, std::memory_order_relaxed);
        }
    }
    auto Pop() -> OCQueueNode*
    {
        auto cur = sentinel.next.exchange(&sentinel, std::memory_order_relaxed);
        if (cur == &sentinel) return nullptr;
        auto nPtr = cur->next.exchange(cur, std::memory_order_relaxed);
        auto expect = cur;
        if (tail.compare_exchange_strong(
            expect, nPtr, std::memory_order_relaxed, std::memory_order_acquire
        )) {
            sentinel.next.store(nPtr, std::memory_order_acquire);
        }
        return cur;
    }
private:
    OCQueueNode sentinel;
    std::atomic<OCQueueNode*> tail;
};

}

#endif // OC_QUEUE_H
