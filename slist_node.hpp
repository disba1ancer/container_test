#ifndef SLIST_NODE_H
#define SLIST_NODE_H

namespace container_test::intrusive {

template <typename Tag = void>
struct SListNode;

template <>
struct SListNode<void> {
    SListNode* next;
};

template <typename Tag>
struct SListNode : SListNode<> {};

template <typename T>
struct SListNodeTraits;

template <typename T>
struct SListNodeTraits<SListNode<T>> {
    using NodeType = SListNode<T>;
    static auto GetNext(NodeType& node) -> NodeType* {
        return node.next;
    }
    static void SetNext(NodeType& node, NodeType* next) {
        node.next = next;
    }
};

}

#endif // SLIST_NODE_H
