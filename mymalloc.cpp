#include <cstdlib>
#include <iostream>
#include <sstream>
#define NOMINMAX 1
#include <windows.h>
#include <utility>
#include "avl_tree.h"
#include "node.h"

#define AddressOf std::addressof

using container_test::ptr_cast;
using container_test::As;
using container_test::ApplyOffset;
using container_test::intrusive::IdentityCastPolicy;

void* my_malloc(std::size_t size);
void my_free(void* ptr);

enum RegionType {
    Free,
    SmallFree,
    Allocated,
    BigAllocated
};

template <typename T>
struct AllocatorRegionTraits;

struct RegionHeader {
    std::size_t type:2;
    std::size_t balance:2;
    std::size_t isLast:1;
    std::size_t size:16;
    std::size_t prev:43;
};

struct FreeHeader {
    RegionHeader header;
    FreeHeader* parent;
    FreeHeader* children[2];
};

struct BigAllocHeader {
    RegionHeader header;
    std::size_t allocOffset;
};

template <>
struct AllocatorRegionTraits<RegionHeader> {
    enum : std::size_t {
        ChunkLogGranularity = 4,
        ChunkLogTreshold = 17,
        ChunkLogSize = 19,
        SizeFieldSize = ChunkLogSize - ChunkLogGranularity + 1,
        ChunkGranularity = std::size_t(1) << ChunkLogGranularity,
        ChunkTreshold = std::size_t(1) << ChunkLogTreshold,
        ChunkSize = std::size_t(1) << ChunkLogSize,
    };

    using FreeHeader = FreeHeader;
private:
    static auto Construct(void* ptr, RegionType type) -> RegionHeader*
    {
        switch (type) {
        case RegionType::SmallFree:
        case RegionType::Allocated:
        case RegionType::BigAllocated:
            return new(ptr) RegionHeader;
        case RegionType::Free:
            return ptr_cast<RegionHeader*>(new(ptr) FreeHeader);
        }
    }
public:
    static auto ConstructChunk(
        void* ptr,
        std::size_t size = ChunkSize,
        std::size_t offset = 0
    ) -> RegionHeader*
    {
        auto rgn = ptr_cast<BigAllocHeader*>(Construct(ptr, RegionType::BigAllocated));
        rgn->allocOffset = offset;
        rgn->header.isLast = true;
        rgn->header.size = size / ChunkGranularity;
        rgn->header.prev = (size / ChunkGranularity) >> SizeFieldSize;
        return ptr_cast<RegionHeader*>(rgn);
    }

    static auto Retype(RegionHeader* region, RegionType type) -> RegionHeader*
    {
        RegionHeader old = *region;
        auto ptr = ptr_cast<unsigned char*>(region);
        Destroy(region);
        region = Construct(ptr, type);
        *region = old;
        region->type = type;
        return region;
    }

    static auto Split(RegionHeader* region, std::size_t firstSize) -> RegionHeader*
    {
        auto ptr = ptr_cast<unsigned char*>(region);
        auto size = GetSize(region);
        auto secondSize = size - firstSize;
        if (firstSize < sizeof(FreeHeader)) {
            region = Retype(region, RegionType::SmallFree);
        }
        region->size = firstSize / ChunkGranularity;
        auto secondType = secondSize < sizeof(FreeHeader) ?
            RegionType::SmallFree : RegionType::Free;
        auto second = Construct(ptr + firstSize, secondType);
        second->size = secondSize / ChunkGranularity;
        second->prev = region->size;
        second->isLast = region->isLast;
        region->isLast = false;
        if (!second->isLast) {
            GetNext(second)->prev = second->size;
        }
        return region;
    }

    static auto MergeWithNext(RegionHeader* region) -> RegionHeader*
    {
        auto next = GetNext(region);
        region->size += next->size;
        region->isLast = next->isLast;
        Destroy(next);
        if (GetSize(region) >= sizeof(FreeHeader)) {
            region = Retype(region, RegionType::Free);
        }
        if (!region->isLast) {
            GetNext(region)->prev = region->size;
        }
        return region;
    }

    static auto Destroy(RegionHeader* region) -> void*
    {
        switch (GetType(region)) {
            case Allocated:
            case SmallFree:
                region->~RegionHeader();
                return region;
            case BigAllocated: {
                auto rgn = ptr_cast<BigAllocHeader*>(region);
                rgn->~BigAllocHeader();
                return rgn;
            }
            case Free: {
                auto rgn = ptr_cast<FreeHeader*>(region);
                rgn->~FreeHeader();
                return rgn;
            }
        }
        return nullptr;
    }

    static int GetType(RegionHeader* header)
    {
        return header->type;
    }

    static auto GetSize(RegionHeader* header) -> std::size_t
    {
        return header->size * ChunkGranularity;
    }

    static auto GetSizeBig(RegionHeader* header) -> std::size_t
    {
        return (header->size | (header->prev << SizeFieldSize)) * ChunkGranularity;
    }

    static auto GetAllocOffset(RegionHeader* header) -> std::size_t
    {
        auto bheader = ptr_cast<BigAllocHeader*>(header);
        return bheader->allocOffset;
    }

    static auto GetNext(RegionHeader* header) -> RegionHeader*
    {
        if (header->isLast) {
            return header;
        }
        auto offset = GetSize(header);
        return ApplyOffset<RegionHeader>(header, std::ptrdiff_t(offset));
    }

    static auto GetPrevSize(RegionHeader* header) -> std::size_t
    {
        return header->prev * ChunkGranularity;
    }

    static auto GetPrev(RegionHeader* header) -> RegionHeader*
    {
        auto offset = GetPrevSize(header);
        return ApplyOffset<RegionHeader>(header, -std::ptrdiff_t(offset));
    }

    static auto AsFreeHeader(RegionHeader* header) -> FreeHeader*
    {
        return ptr_cast<FreeHeader*>(header);
    }

    static auto FromFreeHeader(FreeHeader* header) -> RegionHeader*
    {
        return ptr_cast<RegionHeader*>(header);
    }
};

template <>
struct container_test::intrusive::AVLTreeNodeTraits<FreeHeader> {
    using Tr = AllocatorRegionTraits<RegionHeader>;

    static int GetBalance(FreeHeader& header)
    {
        return int(header.header.balance) - 2;
    }

    static void SetBalance(FreeHeader& header, int balance)
    {
        header.header.balance = std::size_t(balance) + 2;
    }

    static auto GetParent(FreeHeader& header) -> FreeHeader*
    {
        return header.parent;
    }

    static void SetParent(FreeHeader& header, FreeHeader* parent)
    {
        header.parent = parent;
    }

    static auto GetChild(FreeHeader& header, bool right) -> FreeHeader*
    {
        return header.children[right];
    }

    static auto SetChild(FreeHeader& header, bool right, FreeHeader* child)
    {
        header.children[right] = child;
    }
};

template <typename T>
struct alignas(16) AllocatorChunk;

//node {
//    atomic next;
//}

//newNode.next = end;
//prev: pPtr = tail;
//if !cas(pPtr.next, end, newNode) goto prev;
//tail = newNode;

//cur = end.next;
//if cur == end return;
//nPtr = cur;
//swp(cur.next, nPtr);
//cas(tail, cur, nPtr);
//end.next = nPtr;

//newNode.next = end;
//pPtr = newNode;
//swp(tail, pPtr);
//if !cas(pPtr.next, end, newNode) end.next = newNode

//cur = end;
//swp(end.next, cur);
//if cur == end return;
//nPtr = cur
//swp(cur.next, nPtr);
//if cas(tail, cur, nPtr) end.next = nPtr


template <typename T>
class MyAllocator;

template <typename T>
class MyAllocator {
    using Region = T;
    using AllocatorChunk = AllocatorChunk<T>;
    friend AllocatorChunk;
    using RgTr = AllocatorRegionTraits<T>;
    using FreeHeader = typename AllocatorRegionTraits<T>::FreeHeader;
    static constexpr auto ChunkTreshold = RgTr::ChunkTreshold;
    static constexpr auto ChunkSize = RgTr::ChunkSize;

    struct Comparator {
        constexpr
        bool operator()(const FreeHeader& a, const FreeHeader& b) const
        {
            auto aSize = RgTr::GetSize(RgTr::FromFreeHeader(const_cast<FreeHeader*>(AddressOf(a))));
            auto bSize = RgTr::GetSize(RgTr::FromFreeHeader(const_cast<FreeHeader*>(AddressOf(b))));
            return aSize < bSize;
        }
        constexpr
        bool operator()(const FreeHeader& a, std::size_t size) const
        {
            auto aSize = RgTr::GetSize(RgTr::FromFreeHeader(const_cast<FreeHeader*>(AddressOf(a))));
            return aSize < size;
        }
        constexpr
        bool operator()(std::size_t size, const FreeHeader& b) const
        {
            auto bSize = RgTr::GetSize(RgTr::FromFreeHeader(const_cast<FreeHeader*>(AddressOf(b))));
            return size < bSize;
        }
    };

    using SizeTree =
        container_test::intrusive::AVLTree<FreeHeader, Comparator, IdentityCastPolicy<FreeHeader>>;

    auto AllocateBig(std::size_t size, std::size_t align) -> Region*
    {
        auto ptr = VirtualAlloc(nullptr, size + align - RgTr::ChunkGranularity,
            MEM_RESERVE, PAGE_READWRITE);
        if (ptr == nullptr) {
            return nullptr;
        }
        auto offset = ptr_cast<std::uintptr_t>(ptr) & (align - 1);
        auto allocStartOffset = align - offset - RgTr::ChunkGranularity;
        auto bptr = ptr_cast<unsigned char*>(ptr) + allocStartOffset;
        VirtualAlloc(bptr, size, MEM_COMMIT, PAGE_READWRITE);
        Region* rgn = RgTr::ConstructChunk(bptr, size, allocStartOffset);
        RgTr::Retype(rgn, RegionType::BigAllocated);
        return rgn;
    }

    void DeallocateBig(Region* rgn)
    {
        std::size_t offset = 0;
        if (RgTr::GetType(rgn) == RegionType::BigAllocated) {
            offset = RgTr::GetAllocOffset(rgn);
        }
        auto ptr = ptr_cast<unsigned char*>(RgTr::Destroy(rgn)) - offset;
        auto r = VirtualFree(ptr, 0, MEM_RELEASE);
        if (r == FALSE) {
            auto error = GetLastError();
            std::stringstream strstr;
            strstr << "error!" << error;
            throw std::runtime_error(strstr.str());
        }
    }

    auto AllocateChunked(std::size_t size, std::size_t align) -> Region*
    {
        auto rgnIt = freeList.LowerBound(size + align - RgTr::ChunkGranularity);
        Region* rgn;
        if (rgnIt == freeList.End()) {
            rgn = AllocateBig(ChunkSize, RgTr::ChunkGranularity);
            if (rgn == nullptr) {
                return nullptr;
            }
            rgn = RgTr::Retype(rgn, RegionType::SmallFree);
        } else {
            rgn = RgTr::FromFreeHeader(rgnIt.operator->());
            freeList.Erase(rgnIt++);
        }
        auto offset = ptr_cast<std::uintptr_t>(rgn) & (align - 1);
        auto allocStartOffset = align - offset - RgTr::ChunkGranularity;
        if (allocStartOffset > 0) {
            rgn = RgTr::Split(rgn, allocStartOffset);
            if (RgTr::GetType(rgn) == RegionType::Free) {
                freeList.Insert(rgnIt, *RgTr::AsFreeHeader(rgn));
            }
            rgn = RgTr::GetNext(rgn);
        }
        if (RgTr::GetSize(rgn) > size) {
            rgn = RgTr::Split(rgn, size);
            auto rgn2 = RgTr::GetNext(rgn);
            if (RgTr::GetType(rgn2) == RegionType::Free) {
                freeList.Insert(rgnIt, *RgTr::AsFreeHeader(rgn2));
            }
        }
        return RgTr::Retype(rgn, RegionType::Allocated);
    }

    void DeallocateChunked(Region* rgn)
    {
        rgn = RgTr::Retype(rgn, RegionType::Free);
        auto neightbour = RgTr::GetPrev(rgn);
        auto neightbourType = RgTr::GetType(neightbour);
        if (neightbour != rgn && neightbourType != RegionType::Allocated) {
            if (neightbourType == RegionType::Free) {
                freeList.Erase(*RgTr::AsFreeHeader(neightbour));
            }
            rgn = RgTr::MergeWithNext(neightbour);
        }
        neightbour = RgTr::GetNext(rgn);
        neightbourType = RgTr::GetType(neightbour);
        if (neightbour != rgn && neightbourType != RegionType::Allocated) {
            if (neightbourType == RegionType::Free) {
                freeList.Erase(*RgTr::AsFreeHeader(neightbour));
            }
            rgn = RgTr::MergeWithNext(rgn);
        }
        auto size = RgTr::GetSize(rgn);
        if (size >= ChunkSize) {
            DeallocateBig(rgn);
        } else {
            freeList.Insert(*RgTr::AsFreeHeader(rgn));
        }
    }

    bool IsPOT(std::size_t val)
    {
        return !((val - 1) & val);
    }

    void* AllocateChecked(std::size_t size, std::size_t align) {
        size += RgTr::ChunkGranularity;
        Region* rgn;
        if (size < ChunkTreshold) {
            rgn = AllocateChunked(size, align);
        } else {
            rgn = AllocateBig(size, align);
        }
        if (rgn == nullptr) {
            return nullptr;
        }
        auto ptr = ApplyOffset<unsigned char>(rgn, RgTr::ChunkGranularity);
        return new(ptr) unsigned char[size];
    }
public:
    MyAllocator()
    {}
    void* Allocate(std::size_t size, std::size_t align)
    {
        if (size == 0) {
            return nullptr;
        }
        align = std::max(align, std::size_t(RgTr::ChunkGranularity));
        size += RgTr::ChunkGranularity - 1;
        size &= size ^ (RgTr::ChunkGranularity - 1);
        if (!IsPOT(align) || size & (align - 1)) {
            return nullptr;
        }
        return AllocateChecked(size, align);
    }
    void Deallocate(void* ptr)
    {
        if (ptr == nullptr) {
            return;
        }
        auto rgn = ApplyOffset<Region>(ptr, -std::ptrdiff_t(RgTr::ChunkGranularity)); // TODO: Can region be small?
        if (RgTr::GetType(rgn) == RegionType::BigAllocated) {
            DeallocateBig(rgn);
        } else {
            DeallocateChunked(rgn);
        }
    }
    void DumpList()
    {
        std::cout << "Dump\n";
        for (auto &elem : freeList) {
            auto rgn = RgTr::FromFreeHeader(AddressOf(elem));
            std::cout << "Pointer: " << rgn;
            std::cout << "\nSize: " << RgTr::GetSize(rgn);
            std::cout << "\nPrev size: " << RgTr::GetPrevSize(rgn) << "\n\n";
        }
        flush(std::cout);
    }
private:
    SizeTree freeList;
};

MyAllocator<RegionHeader> myAllocator;

void* my_alloc(std::size_t size)
{
    return myAllocator.Allocate(size, alignof(std::max_align_t));
}

void* my_aligned_alloc(std::size_t align, std::size_t size)
{
    return myAllocator.Allocate(size, align);
}

void my_free(void* ptr)
{
    myAllocator.Deallocate(ptr);
}

int main(int argc, char* argv[])
{
    std::cout << std::hex;
    auto pint = new(my_alloc(sizeof(int))) int;
    *pint = 0x55AA;
    std::cout << pint << "\n";
    myAllocator.DumpList();
    void* p = my_aligned_alloc(0x10000, 0x10000);
    std::cout << p << "\n";
    myAllocator.DumpList();
    void* p2 = my_aligned_alloc(0x200000, 0x200000);
    std::cout << p2 << "\n";
    auto iptr = std::launder(static_cast<int*>(p2));
    for (auto i = 0; i < 1024; ++i) {
        iptr[i] = 0x55555555;
    }
    myAllocator.DumpList();
    my_free(pint);
    myAllocator.DumpList();
    my_free(p);
    myAllocator.DumpList();
    my_free(p2);
    myAllocator.DumpList();
}
