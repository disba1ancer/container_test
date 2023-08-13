#ifndef BS_TREE_H
#define BS_TREE_H

#include <cstddef>
#include <utility>
#include <stdexcept>
#include "node.hpp"
#include "bs_tree_node.hpp"

#define AddressOf (::std::addressof)

namespace container_test::intrusive {

namespace detail {

template <typename Comp, typename Key>
concept Comparator = requires (const Key t1, const Key t2, Comp c)
{
    { c(t1, t2) } -> std::same_as<bool>;
};

}

template <
    typename T,
    detail::Comparator<T> Comp,
        typename CastPolicyGen = BaseClassCastPolicy<BSTreeNode<>, T>
>
class BSTree : detail::ContainerNodeRequirments<T, CastPolicyGen> {
public:
    using CastPolicy = CastPolicyGen;
    using NodeType = typename CastPolicyGen::NodeType;
    using NodeTraits = BSTreeNodeTraits<NodeType>;
    using TraitsHelper = BSTreeNodeTraitsHelper<NodeType>;
    BSTree() : root(nullptr)
    {}

    BSTree(BSTree&& oth) :
        root(oth.root)
    {}

    void Insert(T* elem)
    {
        using cp = CastPolicy;
        using H = TraitsHelper;
        auto current = H(root);
        Comp comp;

        auto node = H(cp::ToNode(elem));
        node.Children(0) = nullptr;
        node.Children(1) = nullptr;

        auto t = comp(*cp::FromNode(current), *elem);
        while (current.Children(t) != nullptr) {
            current = current.Children(t);
            t = comp(*cp::FromNode(current), *elem);
        }
        current.Children(t) = node;
        return;
    }

    template <typename KeyType = T>
    T* Extract(KeyType& key)
    {
        using cp = CastPolicy;
        auto r = FindInternal(key);
    }

    T* Find(const T& key) {
        return Find<T>(key);
    }

    template <typename KeyType = T>
    requires(requires (const T& t, const KeyType& k, Comp comp) {
        comp(k, t);
        comp(t, k);
    })
    T* Find(const KeyType& key)
    {
        using cp = CastPolicy;
        auto r = FindInternal(key);
        return cp::FromNode(r.node);
    }

    bool Empty()
    {
        return root == nullptr;
    }
private:
    struct NodeReference {
        NodeType* node;
        NodeType* parent;
    };

    template <typename KeyType = T>
    auto FindInternal(const KeyType& key) -> NodeReference
    {
        using cp = CastPolicy;
        using Tr = NodeTraits;
        Comp comp;

        NodeReference r = { root, nullptr };
        while (r.node != nullptr) {
            auto cptr = cp::FromNode(r.node);
            auto less = comp(*cptr, key);
            if (!(less || comp(key, *cptr))) {
                break;
            }
            r.parent = r.node;
            r.node = Tr::GetChild(*r.node, less);
        }
        return r;
    }

    static auto FindReplacement(NodeType* node) -> NodeReference
    {
        using cp = CastPolicy;
        using Tr = NodeTraits;
        NodeReference r = { Tr::GetChild(*node, 1), node };
        if (r.node == nullptr) {
            r.node = Tr::GetChild(*node, 0);
            return r;
        }
        while (auto child = Tr::GetChild(*r.node, 0)) {
            r.parent = r.node;
            r.node = child;
        }
        return r;
    }

    bool Erase(NodeReference ref) {
        using Tr = NodeTraits;
        auto child0 = Tr::GetChild(*ref.node, 0);
        auto child1 = Tr::GetChild(*ref.node, 1);
        if (child0 != nullptr && child1 != nullptr) {
            return false;
        }
        auto childId = Tr::GetChild(*ref.parent, 1);
    }

    NodeType* root;
};

}

#undef AddressOf

#endif // BS_TREE_H
