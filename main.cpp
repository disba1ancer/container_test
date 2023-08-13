#include <iostream>
#include "hash_table.hpp"
#include "avl_tree.hpp"
#include "slist.hpp"
#include <string>

using namespace std;

template <typename It>
class Range {
public:
    Range(const It& begin, const It& end) :
        m_begin(begin), m_end(end)
    {}

    It Begin() const
    {
        return m_begin;
    }

    It End() const
    {
        return m_end;
    }

    friend It begin(const Range& rng)
    {
        return rng.Begin();
    }

    friend It end(const Range& rng)
    {
        return rng.End();
    }
private:
    It m_begin;
    It m_end;
};

int main(int, char*[])
{
/*    container_test::HashTable<std::string> strs;
    strs[4097] = "Apple";
    strs[1] = "Orange";
    strs[542] = "Hello World!";
    cout << strs[1] << "\n";
    cout << strs[542] << "\n";
    cout << strs[428] << "\n";
    cout << strs[4097] << endl;*/
    struct Elem : container_test::intrusive::AVLTreeNode<> {
        int key;
        std::string value;
        Elem(int key, std::string value) : key(key), value(value) {}
    };
    struct Comp {
        bool operator()(const Elem& a, const Elem& b) const
        {
            return a.key < b.key;
        }
        bool operator()(int a, const Elem& b) const
        {
            return a < b.key;
        }
        bool operator()(const Elem& a, int b) const
        {
            return a.key < b;
        }
    };

    Elem elem[] = {
        { 4097, "Apple" },
        { 1, "Orange" },
        { 542, "Hello World!" },
        { 542, "Lemon" },
        { 428, "Pear" },
    };
    container_test::intrusive::AVLTree<Elem, Comp> tree;
    tree.Insert(elem[0]);
    tree.Insert(elem[1]);
    tree.Insert(elem[2]);
    tree.Insert(elem[3]);
    tree.Insert(elem[4]);
    tree.Erase(542);
//    tree.Erase(tree.Find(elem[4]), tree.Find(elem[3]));
    cout << tree.Find(1)->value << "\n";
    cout << tree.Find(428)->value << "\n";
//    cout << tree.Find(542)->value << "\n";
    cout << tree.Find(4097)->value << endl;
    for (auto& node : tree) {
        cout << node.key << " " << node.value << "\n";
    }
    for (auto& node : Range(tree.LowerBound(542), tree.UpperBound(542))) {
        cout << node.key << " " << node.value << "\n";
    }
    struct Element : container_test::intrusive::SListNode<> {
        std::string str;
    }
    a{ .str = "Apple" },
    b{ .str = "Orange" },
    c{ .str = "Pear" },
    d{ .str = "Hello World!" };
    container_test::intrusive::SList<Element> list;
    list.PushFront(a);
    list.PushFront(b);
    list.PushFront(c);
    list.PushFront(d);
    list.EraseAfter(list.Begin());
    auto l2 = std::move(list);
    for (auto& elem : l2) {
        cout << elem.str << "\n";
    }
    return 0;
}
