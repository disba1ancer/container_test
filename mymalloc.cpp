#include <cstdlib>
#include <iostream>
#include <sstream>
#include <windows.h>
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
    Allocated
};

template <typename T>
struct AllocatorRegionTraits;

struct RegionHeader {
    std::size_t type:3;
    std::size_t balance:3;
    std::size_t isLast:1;
    std::size_t size:20;
    std::size_t prev:20;
};

struct FreeHeader {
    RegionHeader header;
    FreeHeader* parent;
    FreeHeader* children[2];
};

/*struct AllocatedHeader {
    RegionHeader header;
};*/

template <>
struct AllocatorRegionTraits<RegionHeader> {
    enum : std::size_t {
        ChunkLogGranularity = 4,
        ChunkLogTreshold = 17,
        ChunkLogSize = 19,
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
            return new(ptr) RegionHeader;
        case RegionType::Free:
            return ptr_cast<RegionHeader*>(new(ptr) FreeHeader);
        }
    }

    static auto Construct(void* ptr, std::size_t size) -> RegionHeader*
    {
        auto type = (size < sizeof(FreeHeader)) ?
            RegionType::SmallFree : RegionType::Free;
        RegionHeader* rgn = Construct(ptr, type);
        rgn->type = type;
        rgn->size = size / ChunkGranularity;
        return rgn;
    }
public:
    static auto ConstructChunk(void* ptr, std::size_t size = ChunkSize) -> RegionHeader*
    {
        RegionHeader* rgn = Construct(ptr, size);
        rgn->isLast = true;
        return rgn;
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
        auto second = Construct(ptr + firstSize, secondSize);
        second->prev = region->size;
        second->isLast = region->isLast;
        region->isLast = false;
        if (!second->isLast) {
            GetNext(second)->prev = secondSize / ChunkGranularity;
        }
        return region;
    }

    static auto MergeWithNext(RegionHeader* region) -> RegionHeader*
    {
        if (region->isLast) {
            return region;
        }
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
            case SmallFree: {
                auto ptr = As<unsigned char*>(region);
                region->~RegionHeader();
                return ptr;
            }
            case Free: {
                auto ptr = As<unsigned char*>(region);
                auto rgn = ptr_cast<FreeHeader*>(region);
                rgn->~FreeHeader();
                return ptr;
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

    static auto GetNext(RegionHeader* header) -> RegionHeader*
    {
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

    static auto IsLast(RegionHeader* header) -> bool
    {
        return header->isLast;
    }

    static auto AsFreeHeader(RegionHeader* header) -> FreeHeader*
    {
        return ptr_cast<FreeHeader*>(header);
    }

    static auto FromFreeHeader(FreeHeader* header) -> RegionHeader*
    {
        return ptr_cast<RegionHeader*>(header);
    }

    static bool Less(RegionHeader* a, RegionHeader* b)
    {
        return GetSize(a) < GetSize(b);
    }

    static bool Less(RegionHeader* a, std::size_t size)
    {
        return GetSize(a) < size;
    }

    static bool Less(std::size_t size, RegionHeader* b)
    {
        return size < GetSize(b);
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
            return RgTr::GetSize(RgTr::FromFreeHeader(const_cast<FreeHeader*>(AddressOf(a)))) <
                RgTr::GetSize(RgTr::FromFreeHeader(const_cast<FreeHeader*>(AddressOf(b))));
        }
        constexpr
        bool operator()(const FreeHeader& a, std::size_t size) const
        {
            return RgTr::GetSize(RgTr::FromFreeHeader(const_cast<FreeHeader*>(AddressOf(a)))) < size;
        }
        constexpr
        bool operator()(std::size_t size, const FreeHeader& b) const
        {
            return size < RgTr::GetSize(RgTr::FromFreeHeader(const_cast<FreeHeader*>(AddressOf(b))));
        }
    };

    using SizeTree =
        container_test::intrusive::AVLTree<FreeHeader, Comparator, IdentityCastPolicy<FreeHeader>>;

    auto AllocateRaw(std::size_t size) -> void*
    {
        auto ptr = VirtualAlloc(nullptr, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        return new(ptr) unsigned char[size];
    }

    void DeallocateRaw(void* ptr, std::size_t)
    {
        auto r = VirtualFree(ptr, 0, MEM_RELEASE);
        if (r == FALSE) {
            auto error = GetLastError();
            std::stringstream strstr;
            strstr << "error!" << error;
            throw std::runtime_error(strstr.str());
        }
    }

    auto AllocateChunked(std::size_t size) -> Region*
    {
        size += RgTr::ChunkGranularity;
        auto rgnIt = freeList.LowerBound(size);
        Region* rgn;
        if (rgnIt == freeList.End()) {
            auto ptr = AllocateRaw(ChunkSize);
            if (ptr == nullptr) {
                return nullptr;
            }
            rgn = RgTr::ConstructChunk(ptr);
        } else {
            rgn = RgTr::FromFreeHeader(rgnIt.operator->());
            freeList.Erase(rgnIt++);
        }
        if (RgTr::GetSize(rgn) > size) {
            rgn = RgTr::Split(rgn, size);
            freeList.Insert(rgnIt, *RgTr::AsFreeHeader(RgTr::GetNext(rgn)));
        }
        return rgn;
    }

    auto AllocateBig(std::size_t size) -> Region*
    {
        size += RgTr::ChunkGranularity;
        auto ptr = AllocateRaw(size);
        if (ptr == nullptr) {
            return nullptr;
        }
        Region* rgn = RgTr::ConstructChunk(ptr, size);
        return rgn;
    }

    void DeallocateChunked(Region* rgn)
    {
        auto neightbour = RgTr::GetPrev(rgn);
        auto neightbourType = RgTr::GetType(neightbour);
        if (neightbour != rgn && neightbourType != RegionType::Allocated) {
            if (neightbourType == RegionType::Free) {
                freeList.Erase(*RgTr::AsFreeHeader(neightbour));
            }
            rgn = RgTr::MergeWithNext(neightbour);
        }
        if (!RgTr::IsLast(rgn)) {
            neightbour = RgTr::GetNext(rgn);
            if (RgTr::GetType(neightbour) != RegionType::Allocated) {
                if (neightbourType == RegionType::Free) {
                    freeList.Erase(*RgTr::AsFreeHeader(neightbour));
                }
                rgn = RgTr::MergeWithNext(rgn);
            }
        }
        auto size = RgTr::GetSize(rgn);
        if (size >= ChunkSize) {
            DeallocateRaw(RgTr::Destroy(rgn), size);
        } else {
            freeList.Insert(*RgTr::AsFreeHeader(rgn));
        }
    }

    void DeallocateBig(Region* rgn)
    {
        auto size = RgTr::GetSize(rgn);
        RgTr::Destroy(rgn);
        DeallocateRaw(RgTr::Destroy(rgn), size);
    }
public:
    MyAllocator()
    {}
    void* Allocate(std::size_t size, std::size_t align)
    {
        Region* rgn;
        size += RgTr::ChunkGranularity - 1;
        size &= size ^ (RgTr::ChunkGranularity - 1);
        if (size < ChunkTreshold) {
            rgn = AllocateChunked(size);
        } else {
            rgn = AllocateBig(size);
        }
        if (rgn == nullptr) {
            return nullptr;
        }
        RgTr::Retype(rgn, RegionType::Allocated);
        auto ptr = ApplyOffset<unsigned char>(rgn, RgTr::ChunkGranularity);
        return new(ptr) unsigned char[size];
    }
    void Deallocate(void* ptr)
    {
        auto rgn = ApplyOffset<Region>(ptr, -std::ptrdiff_t(RgTr::ChunkGranularity));
        RgTr::Retype(rgn, RegionType::Free);
        if (RgTr::GetSize(rgn) < ChunkTreshold) {
            DeallocateChunked(rgn);
        } else {
            DeallocateBig(rgn);
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

template <typename T>
struct alignas(16) AllocatorChunk {
    using Region = T;
    using RgTr = AllocatorRegionTraits<T>;

    static auto MakeChunk(void* newChunk) -> Region*
    {
        return RgTr::Construct(newChunk, RegionType::Free, RgTr::ChunkSize, 0);
    }
};

MyAllocator<RegionHeader> myAllocator;

void* my_malloc(std::size_t size)
{
    return myAllocator.Allocate(size, 16);
}

void my_free(void* ptr)
{
    myAllocator.Deallocate(ptr);
}

int main(int argc, char* argv[])
{
    auto pint = static_cast<int*>(my_malloc(sizeof(int)));
    *pint = 0x55AA;
    myAllocator.DumpList();
    void* p = my_malloc(0x1FFE0);
    myAllocator.DumpList();
    my_free(pint);
    myAllocator.DumpList();
    my_free(p);
    myAllocator.DumpList();
}
