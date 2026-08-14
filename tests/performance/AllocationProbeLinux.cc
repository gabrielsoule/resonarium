#include "AllocationProbe.h"

#include <cstddef>
#include <cstdint>
#include <new>

// These symbols are supplied by GNU ld/lld when the executable is linked with
// the corresponding -Wl,--wrap=<symbol> options. Calling the __real entry
// points avoids recursion and keeps each C++ allocation to one observed call.
extern "C"
{
void* __real_malloc(std::size_t size);
void* __real_calloc(std::size_t count, std::size_t size);
void* __real_realloc(void* pointer, std::size_t size);
void* __real_aligned_alloc(std::size_t alignment, std::size_t size);
int __real_posix_memalign(void** result, std::size_t alignment, std::size_t size);
void* __real_valloc(std::size_t size);
void* __real_memalign(std::size_t alignment, std::size_t size);
char* __real_strdup(const char* source);
char* __real_strndup(const char* source, std::size_t limit);
void __real_free(void* pointer);
}

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
    if (auto* result = __real_malloc(size == 0 ? 1 : size))
        return result;
    throw std::bad_alloc {};
}

void* allocateAligned(std::size_t size, std::size_t alignment)
{
    recordAllocation();
    void* result = nullptr;
    if (__real_posix_memalign(&result, alignment, size == 0 ? 1 : size) == 0)
        return result;
    throw std::bad_alloc {};
}
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
void operator delete(void* pointer) noexcept { recordDeallocation(pointer); __real_free(pointer); }
void operator delete[](void* pointer) noexcept { recordDeallocation(pointer); __real_free(pointer); }
void operator delete(void* pointer, std::size_t) noexcept { recordDeallocation(pointer); __real_free(pointer); }
void operator delete[](void* pointer, std::size_t) noexcept { recordDeallocation(pointer); __real_free(pointer); }
void operator delete(void* pointer, std::align_val_t) noexcept { recordDeallocation(pointer); __real_free(pointer); }
void operator delete[](void* pointer, std::align_val_t) noexcept { recordDeallocation(pointer); __real_free(pointer); }
void operator delete(void* pointer, std::size_t, std::align_val_t) noexcept { recordDeallocation(pointer); __real_free(pointer); }
void operator delete[](void* pointer, std::size_t, std::align_val_t) noexcept { recordDeallocation(pointer); __real_free(pointer); }

extern "C" void* __wrap_malloc(std::size_t size)
{
    recordAllocation();
    return __real_malloc(size);
}

extern "C" void* __wrap_calloc(std::size_t count, std::size_t size)
{
    recordAllocation();
    return __real_calloc(count, size);
}

extern "C" void* __wrap_realloc(void* pointer, std::size_t size)
{
    recordAllocation();
    return __real_realloc(pointer, size);
}

extern "C" void __wrap_free(void* pointer)
{
    recordDeallocation(pointer);
    __real_free(pointer);
}

extern "C" void* __wrap_aligned_alloc(std::size_t alignment, std::size_t size)
{
    recordAllocation();
    return __real_aligned_alloc(alignment, size);
}

extern "C" int __wrap_posix_memalign(void** result, std::size_t alignment, std::size_t size)
{
    recordAllocation();
    return __real_posix_memalign(result, alignment, size);
}

extern "C" void* __wrap_valloc(std::size_t size)
{
    recordAllocation();
    return __real_valloc(size);
}

extern "C" void* __wrap_memalign(std::size_t alignment, std::size_t size)
{
    recordAllocation();
    return __real_memalign(alignment, size);
}

extern "C" char* __wrap_strdup(const char* source)
{
    recordAllocation();
    return __real_strdup(source);
}

extern "C" char* __wrap_strndup(const char* source, std::size_t limit)
{
    recordAllocation();
    return __real_strndup(source, limit);
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

const char* allocationProbeCoverage() noexcept { return "linux_c_and_cpp_linker_wrap"; }
} // namespace resonarium::test
