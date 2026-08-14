#pragma once

#include <cstdint>

namespace resonarium::test
{
struct AllocationProbeCounts
{
    std::uint64_t allocationCalls = 0;
    std::uint64_t deallocationCalls = 0;

    [[nodiscard]] constexpr std::uint64_t totalCalls() const noexcept
    {
        return allocationCalls + deallocationCalls;
    }
};

/** Counts allocation/deallocation entry points covered by the platform implementation. */
void beginAllocationProbe() noexcept;
[[nodiscard]] AllocationProbeCounts endAllocationProbeCounts() noexcept;

/** Backward-compatible allocation-only result for benchmark call sites. */
[[nodiscard]] std::uint64_t endAllocationProbe() noexcept;
[[nodiscard]] const char* allocationProbeCoverage() noexcept;
} // namespace resonarium::test
