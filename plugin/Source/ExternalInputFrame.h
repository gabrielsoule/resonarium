#pragma once

#include <cmath>
#include <cstdint>

#include <juce_audio_basics/juce_audio_basics.h>

/**
 * Audio-thread-owned host input captured once at the start of a host block.
 *
 * The buffer is allocated in prepare() and is immutable for the remainder of
 * the callback.  Invalid frames are represented by a flag, so stale storage
 * never needs to be cleared from the real-time path.
 */
class ExternalInputFrame final
{
public:
    struct Telemetry
    {
        std::uint64_t hostBlocks = 0;
        std::uint64_t validBlocks = 0;
        std::uint64_t sourceSamplesCopied = 0;
    };

    void prepare(int maximumBlockSize)
    {
        buffer.setSize(2, juce::jmax(1, maximumBlockSize), false, true, false);
        valid = false;
        validSamples = 0;
    }

    /**
     * Captures and trims a stereo source. Returns the dry gain that must be
     * applied to the host buffer before the wet synth output is accumulated.
     */
    [[nodiscard]] float beginBlock(const float* left, const float* right, int numSamples,
                                   bool enabled, float rawMix) noexcept
    {
        ++telemetry.hostBlocks;
        valid = false;
        validSamples = 0;

        const bool storageReady = buffer.getNumChannels() == 2 && numSamples >= 0
                                  && numSamples <= buffer.getNumSamples();
        if (!enabled || left == nullptr || right == nullptr || !storageReady)
            return 1.0f;

        constexpr float sourceTrim = 0.05f;
        buffer.copyFrom(0, 0, left, numSamples, sourceTrim);
        buffer.copyFrom(1, 0, right, numSamples, sourceTrim);
        validSamples = numSamples;
        valid = true;
        ++telemetry.validBlocks;
        telemetry.sourceSamplesCopied += static_cast<std::uint64_t>(numSamples) * 2u;

        return std::cos(rawMix * juce::MathConstants<float>::halfPi);
    }

    [[nodiscard]] bool isValid() const noexcept { return valid; }
    [[nodiscard]] int getNumSamples() const noexcept { return validSamples; }

    [[nodiscard]] const float* getReadPointer(int channel) const noexcept
    {
        jassert(juce::isPositiveAndBelow(channel, 2));
        return juce::isPositiveAndBelow(channel, 2) ? buffer.getReadPointer(channel) : nullptr;
    }

    [[nodiscard]] const Telemetry& getTelemetry() const noexcept { return telemetry; }
    void clearTelemetry() noexcept { telemetry = {}; }

private:
    juce::AudioBuffer<float> buffer;
    int validSamples = 0;
    bool valid = false;
    Telemetry telemetry;
};
