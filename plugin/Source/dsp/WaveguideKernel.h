/*
 * Production contiguous scalar waveguide bank.
 *
 * One instance owns an eight-resonator stereo bank (16 mono lanes), allocates
 * only in prepare(), and exposes raw-pointer process views for the engine's
 * shared scratch arena.
 */

#pragma once

#include "WaveguideDelay.h"

#include <array>
#include <cstddef>
#include <cstdint>

namespace resonarium::waveguide
{
enum class WaveguideCouplingMode : std::uint8_t
{
    parallel,
    interlinked,
    cascade,
};

using WaveguideRetuneMode = WaveguideDelay::RetuneMode;

struct WaveguideFilterControl
{
    float frequencyHz = 1000.0f;
    float resonance = 0.7071067811865476f;
    float mode = 0.0f;
    bool frequencyIsPeak = false;
};

struct WaveguideLaneControl
{
    bool enabled = false;
    float frequencyHz = 440.0f;
    float decaySeconds = 3.0f;
    float dispersion = 0.0f;
    float gain = 1.0f;
    WaveguideFilterControl loopFilter { 3000.0f, 0.70810676f, 0.0f, false };
    WaveguideFilterControl postFilter { 9000.0f, 0.70710678f, 0.0f, false };
};

/** One audio-quantum snapshot; the audio callback does no parameter lookup. */
struct WaveguideControlFrame
{
    static constexpr int resonatorCount = WaveguideDelay::resonatorCount;
    static constexpr int channelCount = WaveguideDelay::channelCount;
    static constexpr int laneCount = WaveguideDelay::laneCount;

    WaveguideCouplingMode couplingMode = WaveguideCouplingMode::parallel;
    std::array<WaveguideLaneControl, laneCount> lanes {};

    float inputMix = 0.0f;
    float inputGain = 1.0f;
    float outputGain = 1.0f;

    std::array<float, channelCount> cascadeAmount {};
    std::array<WaveguideFilterControl, channelCount> cascadeFilter {
        WaveguideFilterControl { 3000.0f, 0.70710678f, 0.0f, false },
        WaveguideFilterControl { 3000.0f, 0.70710678f, 0.0f, false },
    };

    /** -1 disables solo capture; otherwise the selected resonator is 0..7. */
    int soloResonator = -1;
};

/** Cached TPT-SVF coefficients, exposed for focused equivalence tests. */
struct WaveguideFilterCoefficients
{
    float gt0 = 1.0f;
    float gk0 = 1.41421356f;
    float gt1 = 0.0f;
    float gk1 = 0.0f;
    float gt2 = 0.0f;
    float lowpassMix = 1.0f;
    float bandpassMix = 0.0f;
    float highpassMix = 0.0f;
    float outputGain = 1.0f;
};

struct WaveguideLaneControlCache
{
    bool enabled = false;
    bool passthrough = false;
    float frequencyHz = 440.0f;
    float decaySeconds = 3.0f;
    float decayCoefficient = 0.0f;
    float dispersion = 0.0f;
    float dispersionC = 1.0f;
    float dispersionS = 0.0f;
    float delaySamples = 2.0f;
    float gain = 1.0f;
    float loopPhaseDelaySamples = 0.0f;
    WaveguideFilterControl loopControl {};
    WaveguideFilterControl postControl {};
    WaveguideFilterCoefficients loopFilter {};
    WaveguideFilterCoefficients postFilter {};
};

struct WaveguideControlCache
{
    WaveguideCouplingMode couplingMode = WaveguideCouplingMode::parallel;
    std::array<WaveguideLaneControlCache, WaveguideControlFrame::laneCount> lanes {};
    /** Ascending enabled resonator list; only enabled lane gains enter totalGain. */
    std::array<std::uint8_t, WaveguideControlFrame::resonatorCount> enabledIndices {};
    std::uint8_t enabledMask = 0;
    std::uint8_t enabledCount = 0;
    std::uint16_t enabledLaneMask = 0;
    std::uint16_t passthroughMask = 0;
    std::uint16_t retuningMask = 0;
    std::uint16_t coldMask = 0;
    std::array<float, WaveguideControlFrame::channelCount> totalGain {};
    std::array<float, WaveguideControlFrame::channelCount> inverseTotalGain {};
    float exciterMix = 1.0f;
    float previousBankMix = 0.0f;
    float inputGain = 1.0f;
    float outputGain = 1.0f;
    std::array<float, WaveguideControlFrame::channelCount> cascadeAmount {};
    std::array<WaveguideFilterControl, WaveguideControlFrame::channelCount> cascadeControl {};
    std::array<WaveguideFilterCoefficients, WaveguideControlFrame::channelCount> cascadeFilter {};
    int soloResonator = -1;
};

/**
 * Non-owning process buffers. bankInput is writable external scratch: process()
 * fills it once with the mixed/scaled bank input, then every topology consumes
 * it. Both bankInput channels are required. previousBank may be null (silence)
 * and may alias output. If either total gain is zero, output is deliberately
 * left unchanged by the bank-processing contract.
 * Solo buffers are optional and are written only for the selected resonator;
 * callers should clear them before processing.
 */
struct WaveguideProcessView
{
    std::array<const float*, WaveguideControlFrame::channelCount> exciter {};
    std::array<const float*, WaveguideControlFrame::channelCount> previousBank {};
    std::array<float*, WaveguideControlFrame::channelCount> bankInput {};
    std::array<float*, WaveguideControlFrame::channelCount> output {};
    std::array<float*, WaveguideControlFrame::channelCount> solo {};
    int numSamples = 0;
};

class WaveguideKernel
{
public:
    static constexpr int resonatorCount = WaveguideControlFrame::resonatorCount;
    static constexpr int channelCount = WaveguideControlFrame::channelCount;
    static constexpr int laneCount = WaveguideControlFrame::laneCount;
    static constexpr float minimumSupportedFrequencyHz = 10.0f;

    WaveguideKernel() = default;
    WaveguideKernel (const WaveguideKernel&) = delete;
    WaveguideKernel& operator= (const WaveguideKernel&) = delete;
    WaveguideKernel (WaveguideKernel&&) noexcept = default;
    WaveguideKernel& operator= (WaveguideKernel&&) noexcept = default;

    /** Setup-only allocation. Returns false for invalid or unsafe dimensions. */
    [[nodiscard]] bool prepare (double sampleRate, int maximumBlockSize);

    /** O(16) metadata/state reset; the contiguous ring store is not cleared. */
    void reset() noexcept;

    /**
     * Caches one quantum of controls and schedules delay retunes. quantumSamples
     * should equal the samples subsequently passed to process(), or the sum of
     * consecutive process calls before the next update().
     */
    void update (const WaveguideControlFrame& frame,
                 int quantumSamples,
                 WaveguideRetuneMode retuneMode = WaveguideRetuneMode::crossfade) noexcept;

    /** Dispatches to a specialized scalar topology. Allocation-free and lock-free. */
    void process (const WaveguideProcessView& view) noexcept;

    [[nodiscard]] bool isPrepared() const noexcept { return prepared; }
    [[nodiscard]] double getSampleRate() const noexcept { return sampleRate; }
    [[nodiscard]] int getMaximumBlockSize() const noexcept { return maximumBlockSize; }
    [[nodiscard]] int getDelayCapacitySamples() const noexcept { return delay.getCapacitySamples(); }
    [[nodiscard]] std::size_t getDelayStorageBytes() const noexcept { return delay.getAllocatedBytes(); }
    [[nodiscard]] bool isDelayStorageAligned() const noexcept { return delay.isStorageAligned(); }
    [[nodiscard]] std::uint16_t getRetuningLaneMask() const noexcept { return delay.getRetuningMask(); }
    [[nodiscard]] std::uint16_t getColdLaneMask() const noexcept { return delay.getColdMask(); }
    [[nodiscard]] const WaveguideControlCache& getControlCache() const noexcept { return cache; }

    [[nodiscard]] static int calculateDelayCapacity (double sampleRate) noexcept;
    [[nodiscard]] static constexpr int laneIndex (int resonator, int channel) noexcept
    {
        return resonator * channelCount + channel;
    }

private:
    struct FilterState
    {
        float state1 = 0.0f;
        float state2 = 0.0f;
    };

    struct DispersionState
    {
        float previous = 0.0f;
    };

    void processParallel (const WaveguideProcessView& view) noexcept;
    void processInterlinked (const WaveguideProcessView& view) noexcept;
    void processCascade (const WaveguideProcessView& view) noexcept;

    using ProcessFunction = void (WaveguideKernel::*) (const WaveguideProcessView&) noexcept;

    [[nodiscard]] float popLane (int lane) noexcept;
    void pushLane (int lane, float sample) noexcept;
    [[nodiscard]] float postProcessLane (int lane, float sample) noexcept;
    [[nodiscard]] float processCascadeFilter (int lane, float sample) noexcept;
    [[nodiscard]] float processCascadeDcBlocker (int lane, float sample) noexcept;
    void refreshDynamicMasks() noexcept;

    WaveguideDelay delay;
    WaveguideControlCache cache;
    std::array<FilterState, laneCount> loopStates {};
    std::array<FilterState, laneCount> postStates {};
    std::array<FilterState, laneCount> cascadeStates {};
    std::array<DispersionState, laneCount> dispersionStates {};
    std::array<float, laneCount> passthroughSamples {};
    std::array<float, laneCount> cascadeDcInputHistory {};
    std::array<float, laneCount> cascadeDcOutputHistory {};
    std::array<bool, laneCount> laneControlInitialised {};
    std::array<bool, laneCount> loopControlInitialised {};
    std::array<bool, laneCount> postControlInitialised {};
    std::array<bool, laneCount> dispersionControlInitialised {};
    std::array<bool, laneCount> forceSnapOnNextDelayUpdate {};
    std::array<bool, channelCount> cascadeControlInitialised {};
    ProcessFunction processFunction = &WaveguideKernel::processParallel;
    double sampleRate = 0.0;
    int maximumBlockSize = 0;
    bool prepared = false;
};
} // namespace resonarium::waveguide
