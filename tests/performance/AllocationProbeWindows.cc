#include "AllocationProbe.h"

#include <Windows.h>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <malloc.h>
#include <new>

namespace
{
using MallocFunction = void*(__cdecl*)(std::size_t);
using CallocFunction = void*(__cdecl*)(std::size_t, std::size_t);
using ReallocFunction = void*(__cdecl*)(void*, std::size_t);
using FreeFunction = void(__cdecl*)(void*);
using AlignedMallocFunction = void*(__cdecl*)(std::size_t, std::size_t);
using AlignedFreeFunction = void(__cdecl*)(void*);
using HeapAllocFunction = LPVOID(WINAPI*)(HANDLE, DWORD, SIZE_T);
using HeapReAllocFunction = LPVOID(WINAPI*)(HANDLE, DWORD, LPVOID, SIZE_T);
using HeapFreeFunction = BOOL(WINAPI*)(HANDLE, DWORD, LPVOID);
using VirtualAllocFunction = LPVOID(WINAPI*)(LPVOID, SIZE_T, DWORD, DWORD);
using VirtualFreeFunction = BOOL(WINAPI*)(LPVOID, SIZE_T, DWORD);
using RtlAllocateHeapFunction = PVOID(NTAPI*)(PVOID, ULONG, SIZE_T);
using RtlReAllocateHeapFunction = PVOID(NTAPI*)(PVOID, ULONG, PVOID, SIZE_T);
using RtlFreeHeapFunction = BOOLEAN(NTAPI*)(PVOID, ULONG, PVOID);

thread_local bool tracking = false;
thread_local std::uint64_t allocationCount = 0;
thread_local std::uint64_t deallocationCount = 0;
std::atomic<int> hookState { 0 };

MallocFunction originalMalloc = nullptr;
CallocFunction originalCalloc = nullptr;
ReallocFunction originalRealloc = nullptr;
FreeFunction originalFree = nullptr;
AlignedMallocFunction originalAlignedMalloc = nullptr;
AlignedFreeFunction originalAlignedFree = nullptr;
HeapAllocFunction originalHeapAlloc = nullptr;
HeapReAllocFunction originalHeapReAlloc = nullptr;
HeapFreeFunction originalHeapFree = nullptr;
VirtualAllocFunction originalVirtualAlloc = nullptr;
VirtualFreeFunction originalVirtualFree = nullptr;
RtlAllocateHeapFunction originalRtlAllocateHeap = nullptr;
RtlReAllocateHeapFunction originalRtlReAllocateHeap = nullptr;
RtlFreeHeapFunction originalRtlFreeHeap = nullptr;
bool completeIatCoverage = false;
thread_local int nestedObservedOperation = 0;

void recordAllocation() noexcept
{
    if (nestedObservedOperation == 0 && tracking)
        ++allocationCount;
}

void recordDeallocation(void* pointer) noexcept
{
    if (nestedObservedOperation == 0 && tracking && pointer != nullptr)
        ++deallocationCount;
}

struct ScopedObservedOperation final
{
    ScopedObservedOperation() noexcept { ++nestedObservedOperation; }
    ~ScopedObservedOperation() { --nestedObservedOperation; }
};

void* __cdecl trackedMalloc(std::size_t size)
{
    recordAllocation();
    const ScopedObservedOperation scoped;
    return originalMalloc(size);
}

void* __cdecl trackedCalloc(std::size_t count, std::size_t size)
{
    recordAllocation();
    const ScopedObservedOperation scoped;
    return originalCalloc(count, size);
}

void* __cdecl trackedRealloc(void* pointer, std::size_t size)
{
    recordAllocation();
    const ScopedObservedOperation scoped;
    return originalRealloc(pointer, size);
}

void* __cdecl trackedAlignedMalloc(std::size_t size, std::size_t alignment)
{
    recordAllocation();
    const ScopedObservedOperation scoped;
    return originalAlignedMalloc(size, alignment);
}

LPVOID WINAPI trackedHeapAlloc(HANDLE heap, DWORD flags, SIZE_T size)
{
    recordAllocation();
    const ScopedObservedOperation scoped;
    return originalHeapAlloc(heap, flags, size);
}

LPVOID WINAPI trackedHeapReAlloc(HANDLE heap, DWORD flags, LPVOID pointer, SIZE_T size)
{
    recordAllocation();
    const ScopedObservedOperation scoped;
    return originalHeapReAlloc(heap, flags, pointer, size);
}

LPVOID WINAPI trackedVirtualAlloc(LPVOID address, SIZE_T size, DWORD allocationType, DWORD protect)
{
    recordAllocation();
    const ScopedObservedOperation scoped;
    return originalVirtualAlloc(address, size, allocationType, protect);
}

PVOID NTAPI trackedRtlAllocateHeap(PVOID heap, ULONG flags, SIZE_T size)
{
    recordAllocation();
    const ScopedObservedOperation scoped;
    return originalRtlAllocateHeap(heap, flags, size);
}

PVOID NTAPI trackedRtlReAllocateHeap(PVOID heap, ULONG flags, PVOID pointer, SIZE_T size)
{
    recordAllocation();
    const ScopedObservedOperation scoped;
    return originalRtlReAllocateHeap(heap, flags, pointer, size);
}

void __cdecl trackedFree(void* pointer)
{
    recordDeallocation(pointer);
    const ScopedObservedOperation scoped;
    originalFree(pointer);
}

void __cdecl trackedAlignedFree(void* pointer)
{
    recordDeallocation(pointer);
    const ScopedObservedOperation scoped;
    originalAlignedFree(pointer);
}

BOOL WINAPI trackedHeapFree(HANDLE heap, DWORD flags, LPVOID pointer)
{
    recordDeallocation(pointer);
    const ScopedObservedOperation scoped;
    return originalHeapFree(heap, flags, pointer);
}

BOOL WINAPI trackedVirtualFree(LPVOID address, SIZE_T size, DWORD freeType)
{
    recordDeallocation(address);
    const ScopedObservedOperation scoped;
    return originalVirtualFree(address, size, freeType);
}

BOOLEAN NTAPI trackedRtlFreeHeap(PVOID heap, ULONG flags, PVOID pointer)
{
    recordDeallocation(pointer);
    const ScopedObservedOperation scoped;
    return originalRtlFreeHeap(heap, flags, pointer);
}

bool patchImportAddress(const char* symbol, ULONG_PTR replacement, ULONG_PTR& original) noexcept
{
    auto* image = reinterpret_cast<unsigned char*>(GetModuleHandleW(nullptr));
    if (image == nullptr)
        return false;

    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(image);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE)
        return false;

    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(image + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE)
        return false;

    const auto& directory = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    if (directory.VirtualAddress == 0)
        return false;

    bool patched = false;
    auto* descriptor = reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(image + directory.VirtualAddress);
    for (; descriptor->Name != 0; ++descriptor)
    {
        if (descriptor->OriginalFirstThunk == 0 || descriptor->FirstThunk == 0)
            continue;

        auto* names = reinterpret_cast<IMAGE_THUNK_DATA*>(image + descriptor->OriginalFirstThunk);
        auto* functions = reinterpret_cast<IMAGE_THUNK_DATA*>(image + descriptor->FirstThunk);
        for (; names->u1.AddressOfData != 0; ++names, ++functions)
        {
            if (IMAGE_SNAP_BY_ORDINAL(names->u1.Ordinal))
                continue;

            const auto* import = reinterpret_cast<const IMAGE_IMPORT_BY_NAME*>(
                image + names->u1.AddressOfData);
            if (std::strcmp(reinterpret_cast<const char*>(import->Name), symbol) != 0)
                continue;

            auto* slot = &functions->u1.Function;
            if (original == 0)
                original = static_cast<ULONG_PTR>(*slot);

            DWORD oldProtection = 0;
            if (VirtualProtect(slot, sizeof(*slot), PAGE_READWRITE, &oldProtection) == 0)
                continue;
            *slot = static_cast<decltype(functions->u1.Function)>(replacement);
            DWORD ignored = 0;
            VirtualProtect(slot, sizeof(*slot), oldProtection, &ignored);
            FlushInstructionCache(GetCurrentProcess(), slot, sizeof(*slot));
            patched = true;
        }
    }
    return patched;
}

template <typename Function>
bool patchImport(const char* symbol, Function replacement, Function& original) noexcept
{
    ULONG_PTR originalAddress = 0;
    const auto patched = patchImportAddress(
        symbol, reinterpret_cast<ULONG_PTR>(replacement), originalAddress);
    if (original == nullptr && originalAddress != 0)
        original = reinterpret_cast<Function>(originalAddress);
    return patched;
}

void installHooks() noexcept
{
    int expected = 0;
    if (hookState.compare_exchange_strong(expected, 1, std::memory_order_acq_rel))
    {
        const bool mallocPatched = patchImport("malloc", &trackedMalloc, originalMalloc);
        const bool callocPatched = patchImport("calloc", &trackedCalloc, originalCalloc);
        const bool reallocPatched = patchImport("realloc", &trackedRealloc, originalRealloc);
        const bool freePatched = patchImport("free", &trackedFree, originalFree);
        const bool alignedPatched = patchImport(
            "_aligned_malloc", &trackedAlignedMalloc, originalAlignedMalloc);
        const bool alignedFreePatched = patchImport(
            "_aligned_free", &trackedAlignedFree, originalAlignedFree);
        const bool heapAllocPatched = patchImport(
            "HeapAlloc", &trackedHeapAlloc, originalHeapAlloc);
        const bool heapReallocPatched = patchImport(
            "HeapReAlloc", &trackedHeapReAlloc, originalHeapReAlloc);
        const bool heapFreePatched = patchImport(
            "HeapFree", &trackedHeapFree, originalHeapFree);
        patchImport("VirtualAlloc", &trackedVirtualAlloc, originalVirtualAlloc);
        patchImport("VirtualFree", &trackedVirtualFree, originalVirtualFree);
        const bool rtlAllocatePatched = patchImport(
            "RtlAllocateHeap", &trackedRtlAllocateHeap, originalRtlAllocateHeap);
        const bool rtlReallocatePatched = patchImport(
            "RtlReAllocateHeap", &trackedRtlReAllocateHeap, originalRtlReAllocateHeap);
        const bool rtlFreePatched = patchImport(
            "RtlFreeHeap", &trackedRtlFreeHeap, originalRtlFreeHeap);

        const bool crtCoverage = mallocPatched && callocPatched && reallocPatched
                                 && freePatched && alignedPatched && alignedFreePatched;
        const bool staticCrtHeapCoverage = heapAllocPatched && heapReallocPatched
                                           && heapFreePatched;
        const bool staticCrtRtlCoverage = rtlAllocatePatched && rtlReallocatePatched
                                          && rtlFreePatched;
        completeIatCoverage = crtCoverage || staticCrtHeapCoverage || staticCrtRtlCoverage;
        hookState.store(2, std::memory_order_release);
        return;
    }

    while (hookState.load(std::memory_order_acquire) != 2)
        YieldProcessor();
}

void* rawMalloc(std::size_t size) noexcept
{
    if (originalMalloc != nullptr)
        return originalMalloc(size);
    return std::malloc(size);
}

void* rawAlignedMalloc(std::size_t size, std::size_t alignment) noexcept
{
    if (originalAlignedMalloc != nullptr)
        return originalAlignedMalloc(size, alignment);
    return _aligned_malloc(size, alignment);
}

void rawFree(void* pointer) noexcept
{
    if (originalFree != nullptr)
        originalFree(pointer);
    else
        std::free(pointer);
}

void rawAlignedFree(void* pointer) noexcept
{
    if (originalAlignedFree != nullptr)
        originalAlignedFree(pointer);
    else
        _aligned_free(pointer);
}

void* allocate(std::size_t size)
{
    recordAllocation();
    const ScopedObservedOperation scoped;
    if (auto* result = rawMalloc(size == 0 ? 1 : size))
        return result;
    throw std::bad_alloc {};
}

void* allocateAligned(std::size_t size, std::size_t alignment)
{
    recordAllocation();
    const ScopedObservedOperation scoped;
    if (auto* result = rawAlignedMalloc(size == 0 ? 1 : size, alignment))
        return result;
    throw std::bad_alloc {};
}


void deallocate(void* pointer) noexcept
{
    recordDeallocation(pointer);
    const ScopedObservedOperation scoped;
    rawFree(pointer);
}

void deallocateAligned(void* pointer) noexcept
{
    recordDeallocation(pointer);
    const ScopedObservedOperation scoped;
    rawAlignedFree(pointer);
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
void operator delete(void* pointer) noexcept { deallocate(pointer); }
void operator delete[](void* pointer) noexcept { deallocate(pointer); }
void operator delete(void* pointer, std::size_t) noexcept { deallocate(pointer); }
void operator delete[](void* pointer, std::size_t) noexcept { deallocate(pointer); }
void operator delete(void* pointer, std::align_val_t) noexcept { deallocateAligned(pointer); }
void operator delete[](void* pointer, std::align_val_t) noexcept { deallocateAligned(pointer); }
void operator delete(void* pointer, std::size_t, std::align_val_t) noexcept { deallocateAligned(pointer); }
void operator delete[](void* pointer, std::size_t, std::align_val_t) noexcept { deallocateAligned(pointer); }

namespace resonarium::test
{
void beginAllocationProbe() noexcept
{
    // Hook installation is deliberately completed before tracking begins. For
    // Resonarium's /MT build, the executable imports HeapAlloc/HeapReAlloc from
    // the OS even though malloc itself has no IAT entry, so the same probe still
    // observes the static CRT's C allocation path.
    installHooks();
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

const char* allocationProbeCoverage() noexcept
{
    installHooks();
    return completeIatCoverage ? "windows_iat_and_cpp_interposition"
                               : "windows_cpp_interposition_static_crt_no_iat";
}
} // namespace resonarium::test
