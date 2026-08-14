#include "AllocationProbe.h"

#include <atomic>
#include <cstdlib>
#include <new>

#if defined(_WIN32)
 #include <malloc.h>
#endif

namespace
{
thread_local bool tracking = false;
thread_local std::uint64_t allocationCount = 0;
thread_local std::uint64_t deallocationCount = 0;

void recordAllocation() noexcept
{
    if (tracking)
        ++allocationCount;
}

void recordDeallocation(void* pointer) noexcept
{
    if (tracking && pointer != nullptr)
        ++deallocationCount;
}

void* allocate(std::size_t size)
{
    recordAllocation();
    if (void* result = std::malloc(size == 0 ? 1 : size))
        return result;
    throw std::bad_alloc {};
}

void* allocateAligned(std::size_t size, std::size_t alignment)
{
    recordAllocation();
#if defined(_WIN32)
    if (void* result = _aligned_malloc(size == 0 ? 1 : size, alignment))
        return result;
#else
    void* result = nullptr;
    if (posix_memalign(&result, alignment, size == 0 ? 1 : size) == 0)
        return result;
#endif
    throw std::bad_alloc {};
}

void deallocate(void* pointer) noexcept
{
    recordDeallocation(pointer);
    std::free(pointer);
}

#if defined(_WIN32)
void deallocateAligned(void* pointer) noexcept
{
    recordDeallocation(pointer);
    _aligned_free(pointer);
}
#endif
} // namespace

void* operator new(std::size_t size) { return allocate(size); }
void* operator new[](std::size_t size) { return allocate(size); }
void* operator new(std::size_t size, const std::nothrow_t&) noexcept
{
    try { return allocate(size); } catch (...) { return nullptr; }
}
void* operator new[](std::size_t size, const std::nothrow_t&) noexcept
{
    try { return allocate(size); } catch (...) { return nullptr; }
}
void operator delete(void* pointer) noexcept { deallocate(pointer); }
void operator delete[](void* pointer) noexcept { deallocate(pointer); }
void operator delete(void* pointer, std::size_t) noexcept { deallocate(pointer); }
void operator delete[](void* pointer, std::size_t) noexcept { deallocate(pointer); }

void* operator new(std::size_t size, std::align_val_t alignment)
{
    return allocateAligned(size, static_cast<std::size_t>(alignment));
}

void* operator new[](std::size_t size, std::align_val_t alignment)
{
    return allocateAligned(size, static_cast<std::size_t>(alignment));
}
void* operator new(std::size_t size, std::align_val_t alignment, const std::nothrow_t&) noexcept
{
    try { return allocateAligned(size, static_cast<std::size_t>(alignment)); } catch (...) { return nullptr; }
}
void* operator new[](std::size_t size, std::align_val_t alignment, const std::nothrow_t&) noexcept
{
    try { return allocateAligned(size, static_cast<std::size_t>(alignment)); } catch (...) { return nullptr; }
}

#if defined(_WIN32)
void operator delete(void* pointer, std::align_val_t) noexcept { deallocateAligned(pointer); }
void operator delete[](void* pointer, std::align_val_t) noexcept { deallocateAligned(pointer); }
void operator delete(void* pointer, std::size_t, std::align_val_t) noexcept { deallocateAligned(pointer); }
void operator delete[](void* pointer, std::size_t, std::align_val_t) noexcept { deallocateAligned(pointer); }
#else
void operator delete(void* pointer, std::align_val_t) noexcept { deallocate(pointer); }
void operator delete[](void* pointer, std::align_val_t) noexcept { deallocate(pointer); }
void operator delete(void* pointer, std::size_t, std::align_val_t) noexcept { deallocate(pointer); }
void operator delete[](void* pointer, std::size_t, std::align_val_t) noexcept { deallocate(pointer); }
#endif

namespace resonarium::test
{
void beginAllocationProbe() noexcept
{
    allocationCount = 0;
    deallocationCount = 0;
    tracking = true;
}

AllocationProbeCounts endAllocationProbeCounts() noexcept
{
    tracking = false;
    return { allocationCount, deallocationCount };
}

std::uint64_t endAllocationProbe() noexcept { return endAllocationProbeCounts().allocationCalls; }

const char* allocationProbeCoverage() noexcept { return "cxx_new_only"; }
} // namespace resonarium::test
