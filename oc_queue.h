#ifndef OC_QUEUE_H
#define OC_QUEUE_H

#include <atomic>

namespace container_test {

struct OCQueueNode {
    OCQueueNode* next;
};

// One consumer multiple producer queue
struct OCQueue {
    OCQueue() noexcept :
        sentinel{&guard},
        guard{&sentinel},
        tail(&guard),
        size(0)
    {}
    void Push(OCQueueNode* newNode) noexcept
    {
        newNode->next = &sentinel;
        auto pPtr = tail.exchange(newNode, std::memory_order_acq_rel);
        pPtr->next = newNode;
        size.fetch_add(1, std::memory_order_release);
    }
    auto Pop() noexcept -> OCQueueNode*
    {
        if (size.load(std::memory_order_relaxed) == 0) {
            return nullptr;
        }
        while (true) {
            size.fetch_sub(1, std::memory_order_acquire);
            auto cur = PopUnrestr();
            if (cur != &guard) {
                return cur;
            }
            Push(&guard);
        }
    }
private:
    auto PopUnrestr() noexcept -> OCQueueNode*
    {
        auto cur = sentinel.next;
        auto nPtr = cur->next;
        sentinel.next = nPtr;
        return cur;
    }
    OCQueueNode sentinel;
    OCQueueNode guard;
    std::atomic<OCQueueNode*> tail;
    std::atomic_size_t size;
};

}

#endif // OC_QUEUE_H
