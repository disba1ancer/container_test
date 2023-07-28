#ifndef BS_TREE_NODE_H
#define BS_TREE_NODE_H

#include <type_traits>

namespace container_test::intrusive {

template <typename Tag = void>
struct BSTreeNode;

template <>
struct BSTreeNode<void> {
    BSTreeNode* parent;
    BSTreeNode* children[2];
    int balance;
    BSTreeNode() {};
};

template <typename Tag>
struct BSTreeNode : BSTreeNode<> {};

template <typename T>
struct BSTreeNodeTraits;

template <typename T>
struct BSTreeNodeTraits<BSTreeNode<T>> {
    static auto GetChild(BSTreeNode<T>& node, bool right) -> BSTreeNode<T>*
    {
        return node.children[right];
    }
    static void SetChild(BSTreeNode<T>& node, bool right, BSTreeNode<T>* child)
    {
        node.children[right] = child;
    }
};

namespace bs_tree_detail {

struct MoveConstructible {
    MoveConstructible() = default;
    MoveConstructible(const MoveConstructible&) = delete;
    MoveConstructible(MoveConstructible&&) = default;
    MoveConstructible& operator=(const MoveConstructible&) = delete;
    MoveConstructible& operator=(MoveConstructible&&)& = delete;
};

template <typename Drvd, typename T>
struct Comparable : MoveConstructible {
    using ValueType = T;
    operator ValueType() const
    {
        return +*Get();
    }
private:
    Drvd* Get()
    {
        return static_cast<Drvd*>(this);
    }
    auto Get() const -> const Drvd*
    {
        return static_cast<const Drvd*>(this);
    }
};

template <typename T>
struct ChildrenHelper;

template <typename T>
struct RecoursiveHelper;

template <template <typename> typename P, typename N>
struct RecoursiveHelper<P<N>> {
    using Derived = P<N>;
    using NodeType = N;
    auto Children(bool right) -> ChildrenHelper<NodeType>
    {
        return { (NodeType*)(*Get()), right };
    }
private:
    Derived* Get()
    {
        return static_cast<Derived*>(this);
    }
};

template <typename T>
struct ChildrenHelper :
    RecoursiveHelper<ChildrenHelper<T>>,
    Comparable<ChildrenHelper<T>, T*>
{
    using NodeType = T;
    using NodeTraits = BSTreeNodeTraits<NodeType>;
    ChildrenHelper(NodeType* node, bool right) :
        node(node),
        right(right)
    {}
    auto operator=(NodeType* newNode) -> NodeType*
    {
        NodeTraits::SetChild(*node, right, newNode);
        return newNode;
    }
    auto operator=(const ChildrenHelper& hlp) -> ChildrenHelper&
    {
        *this = +hlp;
        return *this;
    }
    auto operator+() const -> NodeType*
    {
        return NodeTraits::GetChild(*node, right);
    }
private:
    NodeType* node;
    bool right;
};

}

template <typename T>
struct BSTreeNodeTraitsHelper :
    bs_tree_detail::RecoursiveHelper<BSTreeNodeTraitsHelper<T>>,
    bs_tree_detail::Comparable<BSTreeNodeTraitsHelper<T>, T*>
{
public:
    using NodeType = T;
    BSTreeNodeTraitsHelper(NodeType* node) : node(node)
    {}
    auto operator=(NodeType* newNode) -> NodeType*
    {
        return node = newNode;
    }
    auto operator+() const -> NodeType*
    {
        return node;
    }
private:
    NodeType* node;
};

}

#endif // BS_TREE_NODE_H
