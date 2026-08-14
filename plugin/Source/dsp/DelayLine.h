/*
 * SPDX-FileCopyrightText: 2020 Raw Material Software Limited
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * Scalar third-/fifth-order Lagrange delay line adapted from chowdsp::DelayLine in
 * ChowDSP Utils revision a8707f8d19af2fdefeac8bb908922b74f16f81c9. That
 * implementation is based on JUCE's DelayLine and retains Raw Material
 * Software's GPLv3 licensing option. The doubled-buffer layout and interpolation
 * policy here follow the inspected ChowDSP implementation; ChowDSP is not a
 * dependency of this header.
 */

#pragma once

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <concepts>
#include <cstddef>
#include <vector>

namespace resonarium
{
enum class DelayInterpolation
{
    lagrange3rd,
    lagrange5th,
};

/**
 * A scalar delay line with selectable Lagrange interpolation.
 *
 * The delay coefficients are shared by all channels, while storage and cursors
 * are independent for each channel. Call prepare() before processing.
 */
template <std::floating_point SampleType = float,
          DelayInterpolation interpolation = DelayInterpolation::lagrange3rd>
class DelayLine
{
public:
    /** Constructs a delay line accepting delays up to maximumDelayInSamples. */
    explicit DelayLine (int maximumDelayInSamples)
        : totalSize (std::max (4, maximumDelayInSamples + 1)),
          channelStride (2 * totalSize + interpolationGuardSize)
    {
        assert (maximumDelayInSamples >= 0);
        setDelay (SampleType { 0 });
    }

    DelayLine (const DelayLine&) = delete;
    DelayLine& operator= (const DelayLine&) = delete;
    DelayLine (DelayLine&&) noexcept = default;
    DelayLine& operator= (DelayLine&&) noexcept = default;

    /** Allocates independent storage and cursors for the requested channels. */
    void prepare (std::size_t numChannels)
    {
        assert (numChannels > 0);

        bufferData.assign (numChannels * static_cast<std::size_t> (channelStride), SampleType { 0 });
        writePositions.assign (numChannels, 0);
        readPositions.assign (numChannels, 0);
        writtenSamplesSinceReset.assign (numChannels, 0);
    }

    /**
     * Logically clears signal state while retaining storage and the selected
     * delay. Each stale backing sample reads as zero until that ring position
     * has been overwritten, matching a physical buffer clear without doing
     * work proportional to the maximum delay on the audio thread.
     */
    void reset() noexcept
    {
        std::fill (writePositions.begin(), writePositions.end(), 0);
        std::fill (readPositions.begin(), readPositions.end(), 0);
        std::fill (writtenSamplesSinceReset.begin(), writtenSamplesSinceReset.end(), 0);
    }

    /** Sets the delay in samples, clamping to the constructor's supported range. */
    void setDelay (SampleType newDelayInSamples) noexcept
    {
        const auto upperLimit = static_cast<SampleType> (totalSize - 1);
        assert (newDelayInSamples >= SampleType { 0 } && newDelayInSamples <= upperLimit);

        const auto clampedDelay = std::clamp (newDelayInSamples, SampleType { 0 }, upperLimit);
        if (delayIsInitialised && clampedDelay == delay)
            return;

        delay = clampedDelay;
        delayIsInitialised = true;
        delayInteger = static_cast<int> (std::floor (delay));
        delayFraction = delay - static_cast<SampleType> (delayInteger);

        // Centre the interpolation taps exactly as ChowDSP's third- and
        // fifth-order policies do, preserving the original effect/resonator
        // sound while retaining this class's O(1) logical reset.
        if constexpr (interpolation == DelayInterpolation::lagrange5th)
        {
            if (delayInteger >= 2)
            {
                delayFraction += SampleType { 2 };
                delayInteger -= 2;
            }
        }
        else
        {
            if (delayInteger >= 1)
            {
                delayFraction++;
                delayInteger--;
            }
        }

        const auto d1 = delayFraction - SampleType { 1 };
        const auto d2 = delayFraction - SampleType { 2 };
        const auto d3 = delayFraction - SampleType { 3 };

        if constexpr (interpolation == DelayInterpolation::lagrange5th)
        {
            const auto d4 = delayFraction - SampleType { 4 };
            const auto d5 = delayFraction - SampleType { 5 };
            interpolationCoefficients[0] = -d1 * d2 * d3 * d4 * d5 / SampleType { 120 };
            interpolationCoefficients[1] = d2 * d3 * d4 * d5 / SampleType { 24 };
            interpolationCoefficients[2] = -d1 * d3 * d4 * d5 / SampleType { 12 };
            interpolationCoefficients[3] = d1 * d2 * d4 * d5 / SampleType { 12 };
            interpolationCoefficients[4] = -d1 * d2 * d3 * d5 / SampleType { 24 };
            interpolationCoefficients[5] = d1 * d2 * d3 * d4 / SampleType { 120 };
        }
        else
        {
            interpolationCoefficients[0] = -d1 * d2 * d3 / SampleType { 6 };
            interpolationCoefficients[1] = d2 * d3 * SampleType { 0.5 };
            interpolationCoefficients[2] = -d1 * d3 * SampleType { 0.5 };
            interpolationCoefficients[3] = d1 * d2 / SampleType { 6 };
        }
    }

    [[nodiscard]] SampleType getDelay() const noexcept { return delay; }

   #if defined (RESONARIUM_TEST_INSTRUMENTATION)
    /** Number of independently prepared channels in this delay instance. */
    [[nodiscard]] std::size_t getPreparedNumChannelsForTest() const noexcept
    {
        return writePositions.size();
    }
   #endif

    /** Writes a sample and advances the selected channel's write cursor. */
    void pushSample (std::size_t channel, SampleType sample) noexcept
    {
        assert (channel < writePositions.size());

        const auto channelOffset = channel * static_cast<std::size_t> (channelStride);
        const auto writePosition = writePositions[channel];
        bufferData[channelOffset + static_cast<std::size_t> (writePosition)] = sample;
        bufferData[channelOffset + static_cast<std::size_t> (writePosition + totalSize)] = sample;
        writtenSamplesSinceReset[channel] = std::min (totalSize, writtenSamplesSinceReset[channel] + 1);
        incrementWritePointer (channel);
    }

    /** Reads using the current delay and advances the selected read cursor. */
    [[nodiscard]] SampleType popSample (std::size_t channel) noexcept
    {
        assert (channel < readPositions.size());

        const auto result = interpolateSample (channel);
        incrementReadPointer (channel);
        return result;
    }

    /** Sets a delay, reads a sample, and optionally advances the read cursor. */
    [[nodiscard]] SampleType popSample (std::size_t channel,
                                        SampleType delayInSamples,
                                        bool updateReadPointer) noexcept
    {
        assert (channel < readPositions.size());

        setDelay (delayInSamples);
        const auto result = interpolateSample (channel);

        if (updateReadPointer)
            incrementReadPointer (channel);

        return result;
    }

    /** Advances a read cursor without producing a sample. */
    void incrementReadPointer (std::size_t channel) noexcept
    {
        assert (channel < readPositions.size());

        auto newReadPosition = readPositions[channel] + totalSize - 1;
        newReadPosition = newReadPosition >= totalSize ? newReadPosition - totalSize : newReadPosition;
        readPositions[channel] = newReadPosition;
    }

    /** Returns true when every logically current ring sample is zero. */
    [[nodiscard]] bool isClear() const noexcept
    {
        for (std::size_t channel = 0; channel < writtenSamplesSinceReset.size(); ++channel)
        {
            const auto writtenSamples = writtenSamplesSinceReset[channel];
            if (writtenSamples == 0)
                continue;

            const auto channelOffset = channel * static_cast<std::size_t> (channelStride);
            if (bufferData[channelOffset] != SampleType { 0 })
                return false;

            const auto firstCurrentPosition = totalSize - writtenSamples + 1;
            for (auto position = firstCurrentPosition; position < totalSize; ++position)
                if (bufferData[channelOffset + static_cast<std::size_t> (position)] != SampleType { 0 })
                    return false;
        }

        return true;
    }

private:
    [[nodiscard]] SampleType readCurrentSample (std::size_t channel,
                                                std::size_t channelOffset,
                                                int relativeIndex) const noexcept
    {
        if (relativeIndex >= 2 * totalSize)
            return SampleType { 0 };

        const auto logicalPosition = relativeIndex >= totalSize ? relativeIndex - totalSize : relativeIndex;
        const auto writtenSamples = writtenSamplesSinceReset[channel];

        // Writes start at logical position zero, then move backwards through
        // totalSize - 1, totalSize - 2, ... . Test each tap independently so
        // partially filled interpolation windows match a physically cleared ring.
        const auto isCurrent = writtenSamples > 0
                               && (logicalPosition == 0
                                   || logicalPosition >= totalSize - writtenSamples + 1);
        return isCurrent
                   ? bufferData[channelOffset + static_cast<std::size_t> (relativeIndex)]
                   : SampleType { 0 };
    }

    [[nodiscard]] SampleType interpolateSample (std::size_t channel) const noexcept
    {
        const auto channelOffset = channel * static_cast<std::size_t> (channelStride);
        const auto index = readPositions[channel] + delayInteger;
        const auto readTap = [this, channel, channelOffset, index] (int tap) noexcept
        {
            const auto relativeIndex = index + tap;
            if (relativeIndex >= 2 * totalSize)
                return SampleType { 0 };

            if (writtenSamplesSinceReset[channel] >= totalSize)
                return bufferData[channelOffset + static_cast<std::size_t> (relativeIndex)];

            return readCurrentSample (channel, channelOffset, relativeIndex);
        };

        const auto value1 = readTap (0);
        const auto value2 = readTap (1);
        const auto value3 = readTap (2);
        const auto value4 = readTap (3);

        if constexpr (interpolation == DelayInterpolation::lagrange5th)
        {
            const auto value5 = readTap (4);
            const auto value6 = readTap (5);

            return value1 * interpolationCoefficients[0]
                   + delayFraction * (value2 * interpolationCoefficients[1]
                                      + value3 * interpolationCoefficients[2]
                                      + value4 * interpolationCoefficients[3]
                                      + value5 * interpolationCoefficients[4]
                                      + value6 * interpolationCoefficients[5]);
        }
        else
        {
            return value1 * interpolationCoefficients[0]
                   + delayFraction * (value2 * interpolationCoefficients[1]
                                      + value3 * interpolationCoefficients[2]
                                      + value4 * interpolationCoefficients[3]);
        }
    }

    void incrementWritePointer (std::size_t channel) noexcept
    {
        auto newWritePosition = writePositions[channel] + totalSize - 1;
        newWritePosition = newWritePosition >= totalSize ? newWritePosition - totalSize : newWritePosition;
        writePositions[channel] = newWritePosition;
    }

    // The inspected Chow buffer's SIMD padding supplies two zero samples after
    // the doubled logical buffer. Both interpolation orders need at most those
    // two guard values at the maximum supported delay.
    static constexpr int interpolationGuardSize = 2;
    int totalSize;
    int channelStride;

    std::vector<SampleType> bufferData;
    std::vector<int> writePositions;
    std::vector<int> readPositions;
    std::vector<int> writtenSamplesSinceReset;

    SampleType delay { 0 };
    SampleType delayFraction { 0 };
    int delayInteger { 0 };
    std::array<SampleType, 6> interpolationCoefficients {};
    bool delayIsInitialised = false;
};
} // namespace resonarium
