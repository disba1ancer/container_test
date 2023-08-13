#ifndef HASH_TABLE_H
#define HASH_TABLE_H

#include <cstddef>
#include <cstdint>
#include <algorithm>
#include <new>
#include <utility>
#include <type_traits>
#include <tuple>

namespace container_test {

namespace impl {

template <typename T>
class HashTableStorage {
    using ValueType = T;

    ValueType* elems;
    std::uint32_t* bitmap;
    std::size_t size;
protected:
    HashTableStorage(std::size_t initSize = 0) :
        elems(nullptr),
        bitmap(nullptr),
        size(initSize)
    {
        if (initSize > 0) {
            elems = static_cast<ValueType*>(::operator new(sizeof(ValueType) * initSize));
            bitmap = new std::uint32_t[initSize / 32];
            std::fill(bitmap, bitmap + initSize / 32, 0);
        }
    }

    ~HashTableStorage()
    {
        if (bitmap != nullptr) {
            Clear();
            delete[] bitmap;
        }
        ::operator delete(elems);
    }

    void ToggleOccupancy(std::size_t pos)
    {
        bitmap[pos >> 5] ^= 1 << (pos & 31);
    }

    bool PosIsOccupied(std::size_t pos) const
    {
        return bitmap[pos >> 5] & (1 << (pos & 31));
    }

    template <typename ... Args>
    bool Emplace(std::size_t pos, Args&& ... args)
    {
        if (pos > size || PosIsOccupied(pos)) {
            return false;
        }
        new(elems + pos) ValueType(std::forward<Args>(args)...);
        ToggleOccupancy(pos);
        return true;
    }

    void Clear()
    {
        for (std::size_t i = 0; i < size; ++i) {
            if (PosIsOccupied(i)) {
                elems[i].~ValueType();
            }
        }
    }

    void Swap(HashTableStorage& other)
    {
        std::swap(elems, other.elems);
        std::swap(bitmap, other.bitmap);
        std::swap(size, other.size);
    }

    void Resize(std::size_t newSize)
    {
        HashTableStorage temp(newSize);
        for (std::size_t i = 0; i < std::min(size, newSize); ++i) {
            if (PosIsOccupied(i)) {
                temp.Emplace(i, (*this)[i]);
            }
        }
        Swap(temp);
    }

    T& operator[](std::size_t pos)
    {
        return elems[pos];
    }

    auto Size() const -> std::size_t
    {
        return size;
    }
public:
    class Iterator {
        friend class HashTableStorage;
        HashTableStorage* storage;
        std::size_t index;

        Iterator(HashTableStorage& storage, std::size_t index) :
            storage(storage),
            index(index)
        {}
    public:
        Iterator& operator++()
        {
            do {
                ++index;
            } while (index != storage->size && !storage->PosIsOccupied(index));
            return *this;
        }

        Iterator operator++(int)
        {
            auto it = *this;
            operator++();
            return it;
        }

        Iterator& operator--()
        {
            do {
                --index;
            } while (!storage->PosIsOccupied(index));
            return *this;
        }

        Iterator operator--(int)
        {
            auto it = *this;
            operator--();
            return it;
        }

        friend bool operator!=(const Iterator& a, const Iterator& b)
        {
            return a.storage != b.storage || a.index != b.index;
        }
    };
};
}

template <typename T, typename Hlp = std::pair<const size_t, T>>
class HashTable : private impl::HashTableStorage<Hlp> {
    using Base = impl::HashTableStorage<Hlp>;
    using ValueType = Hlp;
    using Iterator = typename Base::Iterator;
    std::size_t size;
    static constexpr size_t InitialAllocate = 1024;
public:
    HashTable() :
        Base(InitialAllocate),
        size(0)
    {}

    void Insert(std::size_t hash, const T& val)
    {
        auto mask = Base::Size() - 1;
        std::size_t pos = hash & mask;
        std::size_t i = pos;
        do {
            if (Base::Emplace(i, hash, val)) {
                ++size;
                break;
            }
            i = (i + 1) & mask;
        } while (i != pos && Base::operator[](i).first != i);
    }

    T& operator[](std::size_t hash)
    {
        auto mask = Base::Size() - 1;
        std::size_t pos = hash & mask;
        std::size_t i = pos;
        do {
            if (!Base::PosIsOccupied(i)) {
                break;
            }
            auto& elem = Base::operator[](i);
            if (elem.first == hash) {
                return elem.second;
            }
            i = (i + 1) & mask;
        } while (i != pos && Base::operator[](i).first != i);
        Base::Emplace(i, std::piecewise_construct, std::make_tuple(hash), std::make_tuple());
        size++;
        return Base::operator[](i).second;
    }

    auto Size() const -> std::size_t
    {
        return size;
    }

};

}

#endif // HASH_TABLE_H
