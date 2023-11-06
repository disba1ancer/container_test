#ifndef OC_QUEUE_H
#define OC_QUEUE_H

#include <atomic>
#include <utility>

namespace container_test {

struct DVMPSCQueueNode {
    std::atomic<DVMPSCQueueNode*> next;
};

// One consumer multiple producer queue
struct DVMPSCQueue {
    DVMPSCQueue() noexcept :
        guard{nullptr},
        head{&guard},
        tail(&guard)
    {}
    void Push(DVMPSCQueueNode* newNode) noexcept
    {
        using o = std::memory_order;
        newNode->next.store(nullptr, o::relaxed);
        auto pPtr = tail.exchange(newNode, o::acq_rel);
        pPtr->next.store(newNode, o::release);
    }
    auto Pop() noexcept -> DVMPSCQueueNode*
    {
        using o = std::memory_order;
        if (head == &guard) {
            auto next = guard.next.load(o::relaxed);
            if (next == nullptr) {
                return nullptr;
            }
            PopIntern(next);
        }
        auto next = head->next.load(o::relaxed);
        if (next != nullptr) {
            return PopIntern(next);
        }
        if (head != tail.load(o::relaxed)) {
            return nullptr;
        }
        Push(&guard);
        next = head->next.load(o::relaxed);
        if (next != nullptr) {
            return PopIntern(next);
        }
        return nullptr;
    }
private:
    auto PopIntern(DVMPSCQueueNode* next) noexcept -> DVMPSCQueueNode*
    {
        std::atomic_thread_fence(std::memory_order::acquire);
        return std::exchange(head, next);
    }
    DVMPSCQueueNode guard;
    DVMPSCQueueNode* head;
    std::atomic<DVMPSCQueueNode*> tail;
};

}

#endif // OC_QUEUE_H
