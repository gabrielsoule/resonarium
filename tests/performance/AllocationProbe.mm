#include "AllocationProbe.h"

#include <mach/mach.h>
#include <malloc/malloc.h>

#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>

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
    if (auto* result = malloc_zone_malloc(malloc_default_zone(), size == 0 ? 1 : size))
        return result;
    throw std::bad_alloc {};
}

void* allocateAligned(std::size_t size, std::size_t alignment)
{
    recordAllocation();
    if (auto* result = malloc_zone_memalign(malloc_default_zone(), alignment, size == 0 ? 1 : size))
        return result;
    throw std::bad_alloc {};
}

void deallocate(void* pointer) noexcept
{
    if (pointer != nullptr)
    {
        recordDeallocation(pointer);
        malloc_zone_free(malloc_default_zone(), pointer);
    }
}
} // namespace

__attribute__((visibility("default"))) void* operator new(std::size_t size) { return allocate(size); }
__attribute__((visibility("default"))) void* operator new[](std::size_t size) { return allocate(size); }
__attribute__((visibility("default"))) void* operator new(std::size_t size, const std::nothrow_t&) noexcept
{
    try { return allocate(size); } catch (...) { return nullptr; }
}
__attribute__((visibility("default"))) void* operator new[](std::size_t size, const std::nothrow_t&) noexcept
{
    try { return allocate(size); } catch (...) { return nullptr; }
}
__attribute__((visibility("default"))) void* operator new(std::size_t size, std::align_val_t alignment)
{
    return allocateAligned(size, static_cast<std::size_t>(alignment));
}
__attribute__((visibility("default"))) void* operator new[](std::size_t size, std::align_val_t alignment)
{
    return allocateAligned(size, static_cast<std::size_t>(alignment));
}
__attribute__((visibility("default"))) void* operator new(std::size_t size,
                                                            std::align_val_t alignment,
                                                            const std::nothrow_t&) noexcept
{
    try { return allocateAligned(size, static_cast<std::size_t>(alignment)); } catch (...) { return nullptr; }
}
__attribute__((visibility("default"))) void* operator new[](std::size_t size,
                                                              std::align_val_t alignment,
                                                              const std::nothrow_t&) noexcept
{
    try { return allocateAligned(size, static_cast<std::size_t>(alignment)); } catch (...) { return nullptr; }
}
__attribute__((visibility("default"))) void operator delete(void* pointer) noexcept { deallocate(pointer); }
__attribute__((visibility("default"))) void operator delete[](void* pointer) noexcept { deallocate(pointer); }
__attribute__((visibility("default"))) void operator delete(void* pointer, std::size_t) noexcept { deallocate(pointer); }
__attribute__((visibility("default"))) void operator delete[](void* pointer, std::size_t) noexcept { deallocate(pointer); }
__attribute__((visibility("default"))) void operator delete(void* pointer, std::align_val_t) noexcept
{
    deallocate(pointer);
}
__attribute__((visibility("default"))) void operator delete[](void* pointer, std::align_val_t) noexcept
{
    deallocate(pointer);
}
__attribute__((visibility("default"))) void operator delete(void* pointer, std::size_t, std::align_val_t) noexcept
{
    deallocate(pointer);
}
__attribute__((visibility("default"))) void operator delete[](void* pointer, std::size_t, std::align_val_t) noexcept
{
    deallocate(pointer);
}

extern "C" __attribute__((visibility("default"))) void* malloc(std::size_t size)
{
    recordAllocation();
    return malloc_zone_malloc(malloc_default_zone(), size);
}
extern "C" __attribute__((visibility("default"))) void* calloc(std::size_t count, std::size_t size)
{
    recordAllocation();
    return malloc_zone_calloc(malloc_default_zone(), count, size);
}
extern "C" __attribute__((visibility("default"))) void* realloc(void* pointer, std::size_t size)
{
    recordAllocation();
    return malloc_zone_realloc(malloc_default_zone(), pointer, size);
}
extern "C" __attribute__((visibility("default"))) void* reallocf(void* pointer, std::size_t size)
{
    recordAllocation();
    auto* result = malloc_zone_realloc(malloc_default_zone(), pointer, size);
    if (result == nullptr && pointer != nullptr)
    {
        recordDeallocation(pointer);
        malloc_zone_free(malloc_default_zone(), pointer);
    }
    return result;
}
extern "C" __attribute__((visibility("default"))) void free(void* pointer)
{
    if (pointer != nullptr)
    {
        recordDeallocation(pointer);
        malloc_zone_free(malloc_default_zone(), pointer);
    }
}
extern "C" __attribute__((visibility("default"))) void* valloc(std::size_t size)
{
    recordAllocation();
    return malloc_zone_valloc(malloc_default_zone(), size);
}
extern "C" __attribute__((visibility("default"))) void* aligned_alloc(std::size_t alignment, std::size_t size)
{
    recordAllocation();
    return malloc_zone_memalign(malloc_default_zone(), alignment, size);
}
extern "C" __attribute__((visibility("default"))) int posix_memalign(void** result,
                                                                       std::size_t alignment,
                                                                       std::size_t size)
{
    if (result == nullptr || alignment < sizeof(void*) || (alignment & (alignment - 1)) != 0)
        return EINVAL;
    recordAllocation();
    *result = malloc_zone_memalign(malloc_default_zone(), alignment, size);
    return *result == nullptr ? ENOMEM : 0;
}
extern "C" __attribute__((visibility("default"))) char* strdup(const char* source)
{
    const auto bytes = std::strlen(source) + 1;
    recordAllocation();
    auto* result = static_cast<char*>(malloc_zone_malloc(malloc_default_zone(), bytes));
    if (result != nullptr)
        std::memcpy(result, source, bytes);
    return result;
}
extern "C" __attribute__((visibility("default"))) char* strndup(const char* source, std::size_t limit)
{
    const auto length = strnlen(source, limit);
    recordAllocation();
    auto* result = static_cast<char*>(malloc_zone_malloc(malloc_default_zone(), length + 1));
    if (result != nullptr)
    {
        std::memcpy(result, source, length);
        result[length] = '\0';
    }
    return result;
}

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

const char* allocationProbeCoverage() noexcept { return "darwin_c_and_cpp_interposition"; }
} // namespace resonarium::test
