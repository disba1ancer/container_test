#ifndef SLIST_H
#define SLIST_H

#include "node.h"
#include "slist_node.h"

#define AddressOf (::std::addressof)

namespace container_test::intrusive {

template <typename T, typename CastPolicyGen = BaseClassCastPolicy2<SListNode<>, T>>
class SList;

template <typename T, typename CastPolicyGen>
class SList : detail::ContainerNodeRequirments2<T, CastPolicyGen> {
    using CastPolicy = CastPolicyGen;
    using NodeType = typename CastPolicy::NodeType;
    using NodeTraits = SListNodeTraits<NodeType>;
public:
    class Iterator : public detail::BasicIterator<Iterator, T> {
        friend class SList;
        Iterator(NodeType* ptr) noexcept : ptr(ptr) {}
    public:
        Iterator& operator++() noexcept
        {
            ptr = NodeTraits::GetNext(*ptr);
            return *this;
        }

        T* operator->() const noexcept
        {
            return CastPolicy::FromNode(ptr);
        }

        bool operator==(const Iterator& other) const noexcept
        {
            return this->ptr == other.ptr;
        }
    private:
        NodeType* ptr;
    };

    SList() noexcept {
        Clear();
    }

    SList(SList&& oth) noexcept {
        DoMove(oth);
    }

    SList& operator=(SList&& oth) noexcept {
        DoMove(oth);
        return *this;
    }

    Iterator InsertAfter(Iterator it, T& ref)
        noexcept(noexcept(CastPolicy::ToNode(AddressOf(ref))))
    {
        using Tr = NodeTraits;
        NodeType* elem = CastPolicy::ToNode(AddressOf(ref));
        NodeType* prev = it.ptr;
        NodeType* next = Tr::GetNext(*prev);
        Tr::SetNext(*elem, next);
        Tr::SetNext(*prev, elem);
        return { elem };
    }

    void EraseAfter(Iterator it) noexcept
    {
        using Tr = NodeTraits;
        NodeType* prev = it.ptr;
        NodeType* elem = Tr::GetNext(*prev);
        NodeType* next = Tr::GetNext(*elem);
        Tr::SetNext(*prev, next);
    }

    void EraseAfter(T& ref)
        noexcept(noexcept(IteratorTo(ref)))
    {
        EraseAfter(IteratorTo(ref));
    }

    Iterator BeforeBegin() noexcept
    {
        return { AddressOf(sentinel) };
    }

    Iterator Begin() noexcept
    {
        return { NodeTraits::GetNext(sentinel) };
    }

    Iterator End() noexcept
    {
        return { AddressOf(sentinel) };
    }

    friend Iterator begin(SList& list) noexcept
    {
        return list.Begin();
    }

    friend Iterator end(SList& list) noexcept
    {
        return list.End();
    }

    void PushFront(T& ref) noexcept
    {
        InsertAfter(BeforeBegin(), ref);
    }

    void PopFront() noexcept
    {
        EraseAfter(BeforeBegin());
    }

    static
    auto IteratorTo(T& elem)
        noexcept(noexcept(CastPolicy::ToNode(AddressOf(elem))))
        -> Iterator
    {
        return CastPolicy::ToNode(AddressOf(elem));
    }

    bool Empty() noexcept
    {
        return Begin() == End();
    }

    void Clear()
    {
        NodeTraits::SetNext(sentinel, AddressOf(sentinel));
    }
private:
    void DoMove(SList& oth)
    {
        if (oth.Empty()) {
            Clear();
            return;
        }
        sentinel = oth.sentinel;
        auto next = oth.Begin();
        auto it = next++;
        auto end = oth.End();
        while (next != end) {
            it = next++;
        }
        NodeTraits::SetNext(*(it.ptr), AddressOf(sentinel));
    }

    NodeType sentinel;
};

}

#undef AddressOf

#endif // SLIST_H
