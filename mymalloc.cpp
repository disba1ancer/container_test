#include <cstdlib>
#include <iostream>
#include <sstream>
#define NOMINMAX 1
#include <windows.h>
#include <utility>
#include "allocator.hpp"
#include "node.hpp"

using container_test::ptr_cast;
using container_test::As;
using container_test::ApplyOffset;
using container_test::intrusive::IdentityCastPolicy;
using kernel::memory::AllocatorRegionTraits;
using kernel::memory::Allocator;

void* my_malloc(std::size_t size);
void my_free(void* ptr);

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
            return new(ptr) RegionHeader;
        case RegionType::BigAllocated:
            return ptr_cast<RegionHeader*>(new(ptr) BigAllocHeader);
        case RegionType::Free:
            return ptr_cast<RegionHeader*>(new(ptr) FreeHeader);
        }
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

    static auto ConstructChunk(
        void* ptr,
        std::size_t size,
        std::size_t offset
    ) -> RegionHeader*
    {
        auto rgn = ptr_cast<BigAllocHeader*>(Construct(ptr, RegionType::BigAllocated));
        rgn->allocOffset = offset;
        rgn->header.type = RegionType::BigAllocated;
        rgn->header.isLast = true;
        rgn->header.size = size / ChunkGranularity;
        rgn->header.prev = (size / ChunkGranularity) >> SizeFieldSize;
        return ptr_cast<RegionHeader*>(rgn);
    }
public:
    static auto AllocateChunk(std::size_t size, std::size_t align) -> RegionHeader*
    {
        auto ptr = VirtualAlloc(nullptr, size + align - ChunkGranularity,
            MEM_RESERVE, PAGE_READWRITE);
        if (ptr == nullptr) {
            return nullptr;
        }
        auto offset = ptr_cast<std::uintptr_t>(ptr) & (align - 1);
        auto allocStartOffset = align - offset - ChunkGranularity;
        auto bptr = ptr_cast<unsigned char*>(ptr) + allocStartOffset;
        VirtualAlloc(bptr, size, MEM_COMMIT, PAGE_READWRITE);
        RegionHeader* rgn = ConstructChunk(bptr, size, allocStartOffset);
        return rgn;
    }

    static auto AllocateIdentity(void* ptr) -> RegionHeader*
    {
        return ConstructChunk(ptr, ChunkSize, 0);
    }

    static void DeallocateChunk(RegionHeader* rgn)
    {
        std::size_t offset = 0;
        if (GetType(rgn) == RegionType::BigAllocated) {
            offset = GetAllocOffset(rgn);
        }
        auto ptr = ptr_cast<unsigned char*>(Destroy(rgn)) - offset;
        auto r = VirtualFree(ptr, 0, MEM_RELEASE);
        if (r == FALSE) {
            auto error = GetLastError();
            std::stringstream strstr;
            strstr << "error!" << error;
            throw std::runtime_error(strstr.str());
        }
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

Allocator<RegionHeader> myAllocator;

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
    //myAllocator.DumpList();
    void* p = my_aligned_alloc(0x10000, 0x10000);
    std::cout << p << "\n";
    //myAllocator.DumpList();
    void* p2 = my_aligned_alloc(0x200000, 0x200000);
    std::cout << p2 << "\n";
    auto iptr = std::launder(static_cast<int*>(p2));
    for (auto i = 0; i < 1024; ++i) {
        iptr[i] = 0x55555555;
    }
    //myAllocator.DumpList();
    my_free(pint);
    //myAllocator.DumpList();
    my_free(p);
    //myAllocator.DumpList();
    my_free(p2);
    //myAllocator.DumpList();
}
