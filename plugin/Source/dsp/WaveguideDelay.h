/*
 * Production contiguous scalar waveguide delay storage.
 *
 * One aligned lane-major arena owns the sixteen mono paths. No SIMD-specific
 * code or audio-thread allocation belongs here.
 */

#pragma once

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <new>
#include <type_traits>
#include <vector>

namespace resonarium::waveguide
{
/** Minimal standard allocator used to keep the future SIMD arena explicit. */
template <typename Type, std::size_t alignment>
class WaveguideAlignedAllocator
{
public:
    static_assert (alignment >= alignof (Type));
    static_assert ((alignment & (alignment - 1)) == 0);

    using value_type = Type;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;
    using is_always_equal = std::true_type;

    template <typename Other>
    struct rebind
    {
        using other = WaveguideAlignedAllocator<Other, alignment>;
    };

    WaveguideAlignedAllocator() noexcept = default;

    template <typename Other>
    WaveguideAlignedAllocator (const WaveguideAlignedAllocator<Other, alignment>&) noexcept
    {
    }

    [[nodiscard]] Type* allocate (std::size_t count)
    {
        if (count > std::numeric_limits<std::size_t>::max() / sizeof (Type))
            throw std::bad_array_new_length {};

        return static_cast<Type*> (
            ::operator new (count * sizeof (Type), std::align_val_t { alignment }));
    }

    void deallocate (Type* pointer, std::size_t) noexcept
    {
        ::operator delete (pointer, std::align_val_t { alignment });
    }
};

template <typename Left, typename Right, std::size_t alignment>
[[nodiscard]] constexpr bool operator== (const WaveguideAlignedAllocator<Left, alignment>&,
                                          const WaveguideAlignedAllocator<Right, alignment>&) noexcept
{
    return true;
}

template <typename Left, typename Right, std::size_t alignment>
[[nodiscard]] constexpr bool operator!= (const WaveguideAlignedAllocator<Left, alignment>&,
                                          const WaveguideAlignedAllocator<Right, alignment>&) noexcept
{
    return false;
}

class WaveguideDelay
{
public:
    static constexpr int resonatorCount = 8;
    static constexpr int channelCount = 2;
    static constexpr int laneCount = resonatorCount * channelCount;
    static constexpr std::size_t storageAlignment = 64;

    enum class RetuneMode
    {
        snap,
        crossfade,
    };

    WaveguideDelay() = default;
    WaveguideDelay (const WaveguideDelay&) = delete;
    WaveguideDelay& operator= (const WaveguideDelay&) = delete;
    WaveguideDelay (WaveguideDelay&&) noexcept = default;
    WaveguideDelay& operator= (WaveguideDelay&&) noexcept = default;

    /**
     * Allocates all sixteen lanes. capacitySamples is the complete per-lane
     * stride, including four mirrored head guards; no hidden second ring exists.
     */
    [[nodiscard]] bool prepare (int capacitySamples)
    {
        if (capacitySamples < minimumCapacity)
            return false;

        laneStride = capacitySamples;
        logicalCapacity = laneStride - interpolationGuardCount;
        samples.assign (static_cast<std::size_t> (laneCount * laneStride), 0.0f);
        prepared = true;
        reset();
        return true;
    }

    /** Invalidates every lane without clearing the sample store. */
    void reset() noexcept
    {
        for (int lane = 0; lane < laneCount; ++lane)
            resetLane (lane);
    }

    /** Invalidates one lane and completes any pending retune at its target. */
    void resetLane (int lane) noexcept
    {
        assert (lane >= 0 && lane < laneCount);
        const auto index = static_cast<std::size_t> (lane);
        writePositions[index] = 0;
        readPositions[index] = 0;
        validSamples[index] = 0;

        auto& state = taps[index];
        if (state.initialised)
            state.current = state.target;
        state.fadePosition = 0;
        state.fadeLength = 0;
    }

    /**
     * Selects a new cached fractional-delay tap. Crossfade mode reads the old
     * and new taps in parallel for exactly quantumSamples, including gain 0 and
     * gain 1 endpoints when quantumSamples is greater than one.
     */
    void setDelay (int lane, float delaySamples, int quantumSamples, RetuneMode mode) noexcept
    {
        assert (prepared);
        assert (lane >= 0 && lane < laneCount);

        auto& state = taps[static_cast<std::size_t> (lane)];
        const auto next = makeTap (delaySamples);

        if (! state.initialised || mode == RetuneMode::snap || quantumSamples <= 1)
        {
            state.current = next;
            state.target = next;
            state.fadePosition = 0;
            state.fadeLength = 0;
            state.initialised = true;
            return;
        }

        if (next.delay == state.target.delay)
            return;

        // update() and process() normally use matching quantum lengths. If a
        // caller retargets early, keep whichever stationary tap is dominant.
        if (state.fadeLength > 0 && 2 * state.fadePosition >= state.fadeLength)
            state.current = state.target;

        state.target = next;
        state.fadePosition = 0;
        state.fadeLength = quantumSamples;
    }

    /**
     * Reads one delayed sample and advances the lane cursor. The warmed,
     * stationary path is one validity branch and a direct cached four-tap dot;
     * wraparound is served by the mirrored head guard.
     */
    [[nodiscard]] float popSample (int lane) noexcept
    {
        assert (prepared);
        assert (lane >= 0 && lane < laneCount);

        auto& state = taps[static_cast<std::size_t> (lane)];
        const auto current = readTap (lane, state.current);

        float result = current;
        if (state.fadeLength > 0)
        {
            const auto target = readTap (lane, state.target);
            const auto alpha = state.fadeLength <= 1
                                   ? 1.0f
                                   : static_cast<float> (state.fadePosition)
                                         / static_cast<float> (state.fadeLength - 1);
            result = current + (target - current) * alpha;

            if (++state.fadePosition >= state.fadeLength)
            {
                state.current = state.target;
                state.fadePosition = 0;
                state.fadeLength = 0;
            }
        }

        decrementWrapped (readPositions[static_cast<std::size_t> (lane)]);
        return result;
    }

    /** Writes one sample, maintains the four mirrored guards, and advances. */
    void pushSample (int lane, float sample) noexcept
    {
        assert (prepared);
        assert (lane >= 0 && lane < laneCount);

        const auto index = static_cast<std::size_t> (lane);
        const auto position = writePositions[index];
        const auto offset = laneOffset (lane);
        samples[offset + static_cast<std::size_t> (position)] = sample;

        // Only writes at the logical head need guard maintenance. This branch
        // is cold for all but four samples per complete ring revolution.
        if (position < interpolationGuardCount)
            samples[offset + static_cast<std::size_t> (logicalCapacity + position)] = sample;

        validSamples[index] = std::min (logicalCapacity, validSamples[index] + 1);
        decrementWrapped (writePositions[index]);
    }

    /**
     * Warming-complete stationary block path used by the parallel kernel.
     * Processor receives (delayedSample, sampleIndex) and returns the feedback
     * sample to write. Cursor, tap, validity, and storage metadata are loaded
     * once and committed once; there is no per-sample cold/retune dispatch.
     */
    template <typename Processor>
    void processStationaryLaneBlock (int lane, int numSamples, Processor&& processor) noexcept
    {
        assert (prepared);
        assert (lane >= 0 && lane < laneCount);
        assert (numSamples >= 0);

        const auto laneArrayIndex = static_cast<std::size_t> (lane);
        const auto& state = taps[laneArrayIndex];
        assert (state.fadeLength == 0);
        assert (tapIsWarm (validSamples[laneArrayIndex], state.current));

        auto readPosition = readPositions[laneArrayIndex];
        auto writePosition = writePositions[laneArrayIndex];
        const auto tapInteger = state.current.integer;
        const auto tapFraction = state.current.fraction;
        const auto weights = state.current.weights;
        const auto offset = laneOffset (lane);
        auto* laneSamples = samples.data() + offset;

        for (int sampleIndex = 0; sampleIndex < numSamples; ++sampleIndex)
        {
            auto tapPosition = readPosition + tapInteger;
            if (tapPosition >= logicalCapacity)
                tapPosition -= logicalCapacity;
            const auto* values = laneSamples + tapPosition;
            const auto delayedSample = values[0] * weights[0]
                                       + tapFraction * (values[1] * weights[1]
                                                        + values[2] * weights[2]
                                                        + values[3] * weights[3]);
            const auto feedbackSample = processor (delayedSample, sampleIndex);
            laneSamples[writePosition] = feedbackSample;
            if (writePosition < interpolationGuardCount)
                laneSamples[logicalCapacity + writePosition] = feedbackSample;

            if (--readPosition < 0)
                readPosition = logicalCapacity - 1;
            if (--writePosition < 0)
                writePosition = logicalCapacity - 1;
        }

        readPositions[laneArrayIndex] = readPosition;
        writePositions[laneArrayIndex] = writePosition;
        validSamples[laneArrayIndex] =
            std::min (logicalCapacity, validSamples[laneArrayIndex] + numSamples);
    }

    [[nodiscard]] bool isPrepared() const noexcept { return prepared; }
    /** Complete lane stride, including the four mirrored guard samples. */
    [[nodiscard]] int getCapacitySamples() const noexcept { return laneStride; }
    [[nodiscard]] int getLogicalCapacitySamples() const noexcept { return logicalCapacity; }
    [[nodiscard]] int getLaneStrideSamples() const noexcept { return laneStride; }
    [[nodiscard]] std::size_t getAllocatedBytes() const noexcept { return samples.size() * sizeof (float); }
    [[nodiscard]] const float* getStorageData() const noexcept { return samples.data(); }
    [[nodiscard]] bool isStorageAligned() const noexcept
    {
        return samples.empty()
                   || reinterpret_cast<std::uintptr_t> (samples.data()) % storageAlignment == 0;
    }
    [[nodiscard]] int getValidSamples (int lane) const noexcept
    {
        assert (lane >= 0 && lane < laneCount);
        return validSamples[static_cast<std::size_t> (lane)];
    }
    [[nodiscard]] float getDelay (int lane) const noexcept
    {
        assert (lane >= 0 && lane < laneCount);
        return taps[static_cast<std::size_t> (lane)].target.delay;
    }
    [[nodiscard]] int getRetuneRemainingSamples (int lane) const noexcept
    {
        assert (lane >= 0 && lane < laneCount);
        const auto& state = taps[static_cast<std::size_t> (lane)];
        return state.fadeLength == 0 ? 0 : state.fadeLength - state.fadePosition;
    }
    [[nodiscard]] bool isRetuning (int lane) const noexcept
    {
        return getRetuneRemainingSamples (lane) > 0;
    }
    [[nodiscard]] bool isStationaryWarm (int lane) const noexcept
    {
        return ! isRetuning (lane) && ! isCold (lane);
    }
    [[nodiscard]] bool isCold (int lane) const noexcept
    {
        assert (lane >= 0 && lane < laneCount);
        const auto& state = taps[static_cast<std::size_t> (lane)];
        const auto available = validSamples[static_cast<std::size_t> (lane)];
        return ! tapIsWarm (available, state.current)
               || (state.fadeLength > 0 && ! tapIsWarm (available, state.target));
    }
    [[nodiscard]] std::uint16_t getRetuningMask() const noexcept
    {
        std::uint16_t result = 0;
        for (int lane = 0; lane < laneCount; ++lane)
            if (isRetuning (lane))
                result |= static_cast<std::uint16_t> (1u << lane);
        return result;
    }
    [[nodiscard]] std::uint16_t getColdMask() const noexcept
    {
        std::uint16_t result = 0;
        for (int lane = 0; lane < laneCount; ++lane)
            if (isCold (lane))
                result |= static_cast<std::uint16_t> (1u << lane);
        return result;
    }

private:
    struct CachedTap
    {
        float delay = 2.0f;
        int integer = 1;
        float fraction = 1.0f;
        std::array<float, 4> weights { 0.0f, 1.0f, 0.0f, 0.0f };
    };

    struct TapState
    {
        CachedTap current;
        CachedTap target;
        int fadePosition = 0;
        int fadeLength = 0;
        bool initialised = false;
    };

    [[nodiscard]] CachedTap makeTap (float requestedDelay) const noexcept
    {
        const auto upperLimit = static_cast<float> (logicalCapacity - interpolationTapCount);
        const auto delay = std::clamp (requestedDelay, 2.0f, upperLimit);
        auto delayInteger = static_cast<int> (std::floor (delay));
        auto delayFraction = delay - static_cast<float> (delayInteger);

        if (delayInteger >= 1)
        {
            delayFraction += 1.0f;
            --delayInteger;
        }

        const auto d1 = delayFraction - 1.0f;
        const auto d2 = delayFraction - 2.0f;
        const auto d3 = delayFraction - 3.0f;
        const auto c1 = -d1 * d2 * d3 / 6.0f;
        const auto c2 = d2 * d3 * 0.5f;
        const auto c3 = -d1 * d3 * 0.5f;
        const auto c4 = d1 * d2 / 6.0f;

        return { delay,
                 delayInteger,
                 delayFraction,
                 { c1, c2, c3, c4 } };
    }

    [[nodiscard]] float readTap (int lane, const CachedTap& tap) const noexcept
    {
        const auto laneArrayIndex = static_cast<std::size_t> (lane);
        auto position = readPositions[laneArrayIndex] + tap.integer;
        if (position >= logicalCapacity)
            position -= logicalCapacity;

        const auto* values = samples.data() + laneOffset (lane) + static_cast<std::size_t> (position);
        if (tapIsWarm (validSamples[laneArrayIndex], tap))
            return values[0] * tap.weights[0]
                   + tap.fraction * (values[1] * tap.weights[1]
                                     + values[2] * tap.weights[2]
                                     + values[3] * tap.weights[3]);

        return readColdTap (lane, position, tap);
    }

    [[nodiscard]] float readColdTap (int lane, int position, const CachedTap& tap) const noexcept
    {
        const auto offset = laneOffset (lane);
        const auto value = [this, lane, offset] (int logicalPosition) noexcept
        {
            if (logicalPosition >= logicalCapacity)
                logicalPosition -= logicalCapacity;
            return isLogicallyValid (lane, logicalPosition)
                       ? samples[offset + static_cast<std::size_t> (logicalPosition)]
                       : 0.0f;
        };

        return value (position) * tap.weights[0]
               + tap.fraction * (value (position + 1) * tap.weights[1]
                                 + value (position + 2) * tap.weights[2]
                                 + value (position + 3) * tap.weights[3]);
    }

    [[nodiscard]] static bool tapIsWarm (int written, const CachedTap& tap) noexcept
    {
        // The read and write cursors advance together. Once this many new
        // samples have arrived, all four samples behind this tap are current.
        return written >= tap.integer + interpolationTapCount;
    }

    [[nodiscard]] bool isLogicallyValid (int lane, int logicalPosition) const noexcept
    {
        const auto written = validSamples[static_cast<std::size_t> (lane)];
        return written >= logicalCapacity
               || (written > 0
                   && (logicalPosition == 0
                       || logicalPosition >= logicalCapacity - written + 1));
    }

    [[nodiscard]] std::size_t laneOffset (int lane) const noexcept
    {
        return static_cast<std::size_t> (lane * laneStride);
    }

    void decrementWrapped (int& position) const noexcept
    {
        --position;
        if (position < 0)
            position = logicalCapacity - 1;
    }

    static constexpr int interpolationTapCount = 4;
    static constexpr int interpolationGuardCount = 4;
    static constexpr int minimumCapacity = interpolationTapCount + interpolationGuardCount + 2;

    using SampleStorage = std::vector<float, WaveguideAlignedAllocator<float, storageAlignment>>;
    SampleStorage samples;
    std::array<int, laneCount> writePositions {};
    std::array<int, laneCount> readPositions {};
    std::array<int, laneCount> validSamples {};
    std::array<TapState, laneCount> taps {};
    int logicalCapacity = 0;
    int laneStride = 0;
    bool prepared = false;
};
} // namespace resonarium::waveguide
