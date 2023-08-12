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
        sentinel{{[this, &oth]
        {
            auto beg = oth.sentinel.next.load(std::memory_order_relaxed);
            if (beg == &oth.sentinel) {
                return &sentinel;
            }
            return beg;
        }()}},
        tail([this, &oth]
        {
            auto last = oth.tail.load(std::memory_order_relaxed);
            last->next.store(&sentinel);
            return last;
        }())
    {}
    void Push(OCQueueNode* newNode) noexcept
    {
        newNode->next.store(&sentinel, std::memory_order_relaxed);
        auto pPtr = tail.exchange(newNode, std::memory_order_release);
        auto end = &sentinel;
        if (!sentinel.next.compare_exchange_strong(
            end, newNode, std::memory_order_relaxed, std::memory_order_relaxed
        )) {
            pPtr->next.store(newNode, std::memory_order_relaxed);
        }
    }
    auto Pop() noexcept -> OCQueueNode*
    {
        auto cur = sentinel.next.exchange(&sentinel, std::memory_order_relaxed);
        if (cur == &sentinel) return nullptr;
        auto nPtr = cur->next.exchange(cur, std::memory_order_acquire);
        auto expect = cur;
        if (tail.compare_exchange_strong(
            expect, nPtr, std::memory_order_relaxed, std::memory_order_relaxed
        )) {
            sentinel.next.store(nPtr, std::memory_order_relaxed);
        }
        return cur;
    }
private:
    OCQueueNode sentinel;
    std::atomic<OCQueueNode*> tail;
};

}

#endif // OC_QUEUE_H
