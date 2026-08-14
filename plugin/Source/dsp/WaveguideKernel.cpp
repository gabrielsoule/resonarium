#include "WaveguideKernel.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <bit>
#include <cmath>
#include <complex>
#include <limits>
#include <numbers>

namespace resonarium::waveguide
{
namespace
{
constexpr float inverseRootTwo = 0.70710678118654752440f;
constexpr float interpolationDelaySamples = 1.03f;
constexpr float minimumDecaySeconds = 0.03f;
constexpr float cascadeDcPole = 0.995f;
constexpr double ringSamplesPerHz = 0.15;
constexpr int capacityGuardSamples = 8;
constexpr int minimumDelayCapacitySamples = 10;

// WaveguideDelay currently forms laneCount * laneStride as an int before
// converting it to the vector's size type. Keep every accepted capacity below
// that arithmetic boundary as well as below this API's int return type.
constexpr int maximumDelayCapacitySamples =
    std::numeric_limits<int>::max() / WaveguideDelay::laneCount;
static_assert (maximumDelayCapacitySamples > capacityGuardSamples);

// This target is compiled with fast-math in instrument builds, where
// std::isfinite() may legally be folded to true. Inspecting the IEEE exponent
// keeps setup/control sanitisation effective in those exact production builds.
[[nodiscard]] bool isFinite (float value) noexcept
{
    constexpr std::uint32_t exponentMask = 0x7f800000u;
    return (std::bit_cast<std::uint32_t> (value) & exponentMask) != exponentMask;
}

[[nodiscard]] bool isFinite (double value) noexcept
{
    constexpr std::uint64_t exponentMask = 0x7ff0000000000000ull;
    return (std::bit_cast<std::uint64_t> (value) & exponentMask) != exponentMask;
}

[[nodiscard]] float finiteOr (float value, float fallback) noexcept
{
    return isFinite (value) ? value : fallback;
}

[[nodiscard]] WaveguideCouplingMode sanitiseCouplingMode (
    WaveguideCouplingMode mode) noexcept
{
    switch (mode)
    {
        case WaveguideCouplingMode::parallel:
        case WaveguideCouplingMode::interlinked:
        case WaveguideCouplingMode::cascade:
            return mode;
    }

    return WaveguideCouplingMode::parallel;
}

[[nodiscard]] WaveguideFilterControl sanitiseFilterControl (
    WaveguideFilterControl control) noexcept
{
    control.frequencyHz = std::max (0.0f, finiteOr (control.frequencyHz, 0.0f));
    control.resonance = std::max (std::numeric_limits<float>::epsilon(),
                                  finiteOr (control.resonance, inverseRootTwo));
    control.mode = std::clamp (finiteOr (control.mode, 0.0f), 0.0f, 1.0f);
    return control;
}

struct FilterDesign
{
    WaveguideFilterCoefficients coefficients;
    float g = 0.0f;
    float k = 1.0f / inverseRootTwo;
};

[[nodiscard]] bool controlsEqual (const WaveguideFilterControl& left,
                                  const WaveguideFilterControl& right) noexcept
{
    return left.frequencyHz == right.frequencyHz
           && left.resonance == right.resonance
           && left.mode == right.mode
           && left.frequencyIsPeak == right.frequencyIsPeak;
}

[[nodiscard]] float calculatePeakGain (float lowpassMix,
                                       float bandpassMix,
                                       float highpassMix,
                                       float resonance,
                                       float k) noexcept
{
    if (lowpassMix >= 0.98f || highpassMix >= 0.98f)
    {
        if (resonance > inverseRootTwo)
        {
            const auto kSquared = k * k;
            return 2.0f / (kSquared * std::sqrt (4.0f / kSquared - 1.0f));
        }

        return 1.0f;
    }

    if (bandpassMix == 1.0f)
        return resonance;

    if (resonance < inverseRootTwo)
        return lowpassMix + highpassMix;

    const auto a = static_cast<double> (lowpassMix == 0.0f ? highpassMix : lowpassMix);
    const auto b = static_cast<double> (bandpassMix);
    const auto q = static_cast<double> (resonance);
    const auto qSquared = q * q;
    const auto aSquared = a * a;
    const auto bSquared = b * b;
    const auto term1 = std::sqrt (-aSquared * bSquared
                                  + (aSquared + bSquared) * (aSquared + bSquared) * qSquared);
    const auto term2 = bSquared * (1.0 - 2.0 * qSquared)
                       + 2.0 * q * (-aSquared * q + term1);
    return static_cast<float> (bSquared * q * std::sqrt (1.0 / term2));
}

[[nodiscard]] FilterDesign designFilter (const WaveguideFilterControl& control,
                                         double sampleRate,
                                         bool unityGain) noexcept
{
    FilterDesign result;
    const auto sampleRateFloat = static_cast<float> (sampleRate);
    const auto nyquist = std::nextafter (0.5f * sampleRateFloat, 0.0f);
    const auto frequency = std::clamp (control.frequencyHz, 0.0f, nyquist);
    const auto resonance = std::max (control.resonance, std::numeric_limits<float>::epsilon());
    const auto mode = std::clamp (control.mode, 0.0f, 1.0f);

    const auto lowpassMix = 1.0f - 2.0f * std::min (0.5f, mode);
    const auto bandpassMix = 1.0f - std::abs (2.0f * (mode - 0.5f));
    const auto highpassMix = 2.0f * std::max (0.5f, mode) - 1.0f;
    const auto radians = std::numbers::pi_v<float> * frequency / sampleRateFloat;

    result.k = 1.0f / resonance;
    if (control.frequencyIsPeak)
    {
        const auto denominatorSquared = std::max (
            resonance * resonance - 0.5f,
            std::numeric_limits<float>::min());
        result.g = resonance * std::tan (radians) / std::sqrt (denominatorSquared);
    }
    else
    {
        result.g = std::tan (radians);
    }

    const auto gPlusK = result.g + result.k;
    auto& coefficients = result.coefficients;
    coefficients.gt0 = 1.0f / (1.0f + result.g * gPlusK);
    coefficients.gk0 = gPlusK * coefficients.gt0;
    coefficients.gt1 = result.g * coefficients.gt0;
    coefficients.gk1 = result.g * coefficients.gk0;
    coefficients.gt2 = result.g * coefficients.gt1;
    coefficients.lowpassMix = lowpassMix;
    coefficients.bandpassMix = bandpassMix;
    coefficients.highpassMix = highpassMix;
    coefficients.outputGain = unityGain
                                  ? 1.0f / calculatePeakGain (lowpassMix,
                                                              bandpassMix,
                                                              highpassMix,
                                                              resonance,
                                                              result.k)
                                  : 1.0f;
    return result;
}

[[nodiscard]] float calculatePhaseDelaySamples (const FilterDesign& design,
                                                float responseFrequency,
                                                double sampleRate) noexcept
{
    if (responseFrequency <= 0.0f)
        return 0.0f;

    const auto omega = 2.0f * std::numbers::pi_v<float> * responseFrequency
                       / static_cast<float> (sampleRate);
    const std::complex<float> z = std::exp (std::complex<float> { 0.0f, omega });
    const auto gSquared = design.g * design.g;
    const auto gTimesK = design.g * design.k;
    const auto onePlusZ = 1.0f + z;
    const auto zMinusOne = z - 1.0f;
    const auto zSquaredMinusOne = z * z - 1.0f;
    const auto& coefficients = design.coefficients;

    const auto lowpassNumerator = gSquared * onePlusZ * onePlusZ;
    const auto bandpassNumerator = design.g * zSquaredMinusOne;
    const auto highpassNumerator = zMinusOne * zMinusOne;
    const auto numerator = coefficients.lowpassMix * lowpassNumerator
                           + coefficients.bandpassMix * bandpassNumerator
                           + coefficients.highpassMix * highpassNumerator;
    const auto denominator = zMinusOne * zMinusOne
                             + gSquared * onePlusZ * onePlusZ
                             + gTimesK * zSquaredMinusOne;
    return -std::arg (numerator / denominator) / omega;
}

template <typename State>
[[nodiscard]] inline float processFilter (const WaveguideFilterCoefficients& coefficients,
                                          State& state,
                                          float input) noexcept
{
    const auto t0 = input - state.state2;
    const auto t1 = coefficients.gt1 * t0 - coefficients.gk1 * state.state1;
    const auto t2 = coefficients.gt2 * t0 + coefficients.gt1 * state.state1;
    const auto highpass = coefficients.gt0 * t0 - coefficients.gk0 * state.state1;
    const auto bandpass = t1 + state.state1;
    const auto lowpass = t2 + state.state2;
    const auto output = std::fma (coefficients.lowpassMix,
                                  lowpass,
                                  std::fma (coefficients.bandpassMix,
                                            bandpass,
                                            coefficients.highpassMix * highpass));

    state.state1 += 2.0f * t1;
    state.state2 += 2.0f * t2;
    return output * coefficients.outputGain;
}
} // namespace

int WaveguideKernel::calculateDelayCapacity (double requestedSampleRate) noexcept
{
    if (! isFinite (requestedSampleRate) || requestedSampleRate <= 0.0)
        return 0;

    const auto ringSamples = ringSamplesPerHz * requestedSampleRate;
    constexpr auto maximumRingSamples =
        static_cast<double> (maximumDelayCapacitySamples - capacityGuardSamples);
    if (! isFinite (ringSamples) || ringSamples > maximumRingSamples)
        return 0;

    // Spell the mathematically equivalent 1.5 / 10 scale as one literal.
    // Under -ffast-math, `1.5 * rate / 10` was reassociated through a slightly
    // high reciprocal and ceil() over-allocated one sample at exact integer
    // boundaries (for example 7,209 rather than 7,208 at 48 kHz).
    const auto capacity = static_cast<int> (std::ceil (ringSamples)) + capacityGuardSamples;
    return std::max (minimumDelayCapacitySamples, capacity);
}

bool WaveguideKernel::prepare (double requestedSampleRate,
                                           int requestedMaximumBlockSize)
{
    // prepare() is a setup boundary. Fail closed so an invalid re-prepare can
    // never leave a previous rate's state looking usable to the caller.
    prepared = false;
    sampleRate = 0.0;
    maximumBlockSize = 0;

    if (! isFinite (requestedSampleRate)
        || requestedSampleRate <= 0.0
        || requestedMaximumBlockSize <= 0)
        return false;

    const auto capacity = calculateDelayCapacity (requestedSampleRate);
    if (capacity <= 0 || ! delay.prepare (capacity))
        return false;

    sampleRate = requestedSampleRate;
    maximumBlockSize = requestedMaximumBlockSize;
    cache = {};
    laneControlInitialised.fill (false);
    loopControlInitialised.fill (false);
    postControlInitialised.fill (false);
    dispersionControlInitialised.fill (false);
    cascadeControlInitialised.fill (false);
    processFunction = &WaveguideKernel::processParallel;
    prepared = true;
    reset();
    return true;
}

void WaveguideKernel::reset() noexcept
{
    if (! prepared)
        return;

    delay.reset();
    loopStates = {};
    postStates = {};
    cascadeStates = {};
    dispersionStates = {};
    passthroughSamples.fill (0.0f);
    cascadeDcInputHistory.fill (0.0f);
    cascadeDcOutputHistory.fill (0.0f);
    // A reset is a new assignment boundary. Every lane's first committed
    // effective pitch must start at its target rather than crossfading from a
    // previous note's cached tap. Lanes that remain disabled retain this flag
    // until their first delay commit.
    forceSnapOnNextDelayUpdate.fill (true);
    refreshDynamicMasks();
}

void WaveguideKernel::update (const WaveguideControlFrame& frame,
                                          int quantumSamples,
                                          WaveguideRetuneMode retuneMode) noexcept
{
    assert (prepared);
    if (! prepared)
        return;

    const auto couplingMode = sanitiseCouplingMode (frame.couplingMode);
    if (couplingMode != cache.couplingMode)
    {
        cache.couplingMode = couplingMode;
        switch (couplingMode)
        {
            case WaveguideCouplingMode::parallel:
                processFunction = &WaveguideKernel::processParallel;
                break;
            case WaveguideCouplingMode::interlinked:
                processFunction = &WaveguideKernel::processInterlinked;
                break;
            case WaveguideCouplingMode::cascade:
                processFunction = &WaveguideKernel::processCascade;
                break;
        }
    }

    const auto inputMix = std::clamp (finiteOr (frame.inputMix, 0.0f), 0.0f, 1.0f);
    cache.exciterMix = 1.0f - inputMix;
    cache.previousBankMix = 0.5f * inputMix;
    cache.inputGain = std::max (0.0f, finiteOr (frame.inputGain, 0.0f));
    cache.outputGain = std::max (0.0f, finiteOr (frame.outputGain, 0.0f));
    cache.soloResonator = frame.soloResonator >= 0 && frame.soloResonator < resonatorCount
                              ? frame.soloResonator
                              : -1;
    cache.enabledMask = 0;
    cache.enabledLaneMask = 0;
    cache.passthroughMask = 0;
    cache.enabledCount = 0;
    cache.totalGain.fill (0.0f);

    // Only enabled lane gains participate in normalization. Enabled zero-gain
    // lanes remain active so their internal state continues to advance.
    for (int resonator = 0; resonator < resonatorCount; ++resonator)
    {
        const auto leftLane = laneIndex (resonator, 0);
        const auto rightLane = laneIndex (resonator, 1);
        const auto enabled = frame.lanes[static_cast<std::size_t> (leftLane)].enabled
                             || frame.lanes[static_cast<std::size_t> (rightLane)].enabled;
        if (enabled)
        {
            cache.enabledMask |= static_cast<std::uint8_t> (1u << resonator);
            cache.enabledIndices[cache.enabledCount++] = static_cast<std::uint8_t> (resonator);
        }

        for (int channel = 0; channel < channelCount; ++channel)
        {
            const auto lane = laneIndex (resonator, channel);
            const auto& sourceControl = frame.lanes[static_cast<std::size_t> (lane)];
            const auto gain = std::max (0.0f, finiteOr (sourceControl.gain, 0.0f));
            if (sourceControl.enabled)
                cache.totalGain[static_cast<std::size_t> (channel)] += gain;
            cache.lanes[static_cast<std::size_t> (lane)].enabled = sourceControl.enabled;
            cache.lanes[static_cast<std::size_t> (lane)].gain = gain;
            if (sourceControl.enabled)
                cache.enabledLaneMask |= static_cast<std::uint16_t> (1u << lane);
        }
    }

    for (int channel = 0; channel < channelCount; ++channel)
    {
        const auto total = cache.totalGain[static_cast<std::size_t> (channel)];
        cache.inverseTotalGain[static_cast<std::size_t> (channel)] = total == 0.0f ? 0.0f : 1.0f / total;
        cache.cascadeAmount[static_cast<std::size_t> (channel)] = std::clamp (
            finiteOr (frame.cascadeAmount[static_cast<std::size_t> (channel)], 0.0f),
            -1.0f,
            1.0f);

        const auto control = sanitiseFilterControl (
            frame.cascadeFilter[static_cast<std::size_t> (channel)]);
        if (! cascadeControlInitialised[static_cast<std::size_t> (channel)]
            || ! controlsEqual (control, cache.cascadeControl[static_cast<std::size_t> (channel)]))
        {
            cache.cascadeControl[static_cast<std::size_t> (channel)] = control;
            cache.cascadeFilter[static_cast<std::size_t> (channel)] =
                designFilter (control, sampleRate, false).coefficients;
            cascadeControlInitialised[static_cast<std::size_t> (channel)] = true;
        }
    }

    for (int lane = 0; lane < laneCount; ++lane)
    {
        const auto laneArrayIndex = static_cast<std::size_t> (lane);
        auto control = frame.lanes[laneArrayIndex];
        control.frequencyHz = std::max (minimumSupportedFrequencyHz,
                                        finiteOr (control.frequencyHz,
                                                  minimumSupportedFrequencyHz));
        control.decaySeconds = std::clamp (finiteOr (control.decaySeconds, 0.0f),
                                           0.0f,
                                           60.0f);
        control.dispersion = std::clamp (finiteOr (control.dispersion, 0.0f), 0.0f, 1.0f);
        control.gain = std::max (0.0f, finiteOr (control.gain, 0.0f));
        control.loopFilter = sanitiseFilterControl (control.loopFilter);
        control.postFilter = sanitiseFilterControl (control.postFilter);
        auto& laneCache = cache.lanes[laneArrayIndex];

        if (! control.enabled)
            continue;

        // Commit post-filter controls before the decay-passthrough decision so
        // they are current when a lane returns to normal processing.
        if (! postControlInitialised[laneArrayIndex]
            || ! controlsEqual (control.postFilter, laneCache.postControl))
        {
            laneCache.postControl = control.postFilter;
            laneCache.postFilter = designFilter (control.postFilter, sampleRate, false).coefficients;
            postControlInitialised[laneArrayIndex] = true;
        }

        const auto passthrough = control.decaySeconds < minimumDecaySeconds;
        if (passthrough)
        {
            laneCache.passthrough = true;
            cache.passthroughMask |= static_cast<std::uint16_t> (1u << lane);
            continue;
        }

        if (laneCache.passthrough)
        {
            delay.resetLane (lane);
            passthroughSamples[laneArrayIndex] = 0.0f;
        }
        laneCache.passthrough = false;

        const auto frequency = std::max (control.frequencyHz, minimumSupportedFrequencyHz);
        const auto frequencyChanged = ! laneControlInitialised[laneArrayIndex]
                                      || frequency != laneCache.frequencyHz;
        const auto decayChanged = ! laneControlInitialised[laneArrayIndex]
                                  || control.decaySeconds != laneCache.decaySeconds;
        if (frequencyChanged || decayChanged)
        {
            laneCache.frequencyHz = frequency;
            laneCache.decaySeconds = control.decaySeconds;
            laneCache.decayCoefficient = control.decaySeconds == 60.0f
                                             ? 1.0f
                                             : std::pow (0.001f,
                                                         1.0f / (control.decaySeconds * frequency));
        }

        const auto loopChanged = ! loopControlInitialised[laneArrayIndex]
                                 || ! controlsEqual (control.loopFilter, laneCache.loopControl);
        if (loopChanged)
        {
            laneCache.loopControl = control.loopFilter;
            const auto design = designFilter (control.loopFilter, sampleRate, true);
            laneCache.loopFilter = design.coefficients;
            laneCache.loopPhaseDelaySamples = control.loopFilter.mode == 0.5f
                                                  ? 0.0f
                                                  : calculatePhaseDelaySamples (design,
                                                                                frequency,
                                                                                sampleRate);
            loopControlInitialised[laneArrayIndex] = true;
        }

        if (! dispersionControlInitialised[laneArrayIndex]
            || control.dispersion != laneCache.dispersion)
        {
            laneCache.dispersion = control.dispersion;
            const auto amount = std::clamp (control.dispersion, 0.0f, 1.0f);
            const auto angle = amount * -0.5f * std::numbers::pi_v<float>;
            laneCache.dispersionC = std::cos (angle);
            laneCache.dispersionS = std::sin (angle);
            dispersionControlInitialised[laneArrayIndex] = true;
        }

        const auto assignmentSnapPending = forceSnapOnNextDelayUpdate[laneArrayIndex];
        if (frequencyChanged || loopChanged || assignmentSnapPending)
        {
            // reset() intentionally keeps the hot coefficient/control caches.
            // If a new assignment starts at the same effective pitch as the
            // previous one, frequencyChanged is false, but we must still
            // consume its one mandatory snap here. Otherwise the first later
            // glide/bend would incorrectly inherit the snap and skip its
            // crossfade. Reusing delaySamples avoids redundant phase-delay
            // work on that unchanged-control path.
            auto delaySamples = laneCache.delaySamples;
            if (frequencyChanged || loopChanged)
            {
                delaySamples = static_cast<float> (sampleRate) / frequency
                               - interpolationDelaySamples
                               - laneCache.loopPhaseDelaySamples;
                delaySamples = std::max (2.0f, delaySamples);
            }
            const auto selectedRetuneMode = assignmentSnapPending
                                                ? WaveguideRetuneMode::snap
                                                : retuneMode;
            delay.setDelay (lane, delaySamples, quantumSamples, selectedRetuneMode);
            forceSnapOnNextDelayUpdate[laneArrayIndex] = false;
            laneCache.delaySamples = delay.getDelay (lane);
        }
        laneControlInitialised[laneArrayIndex] = true;
    }

    refreshDynamicMasks();
}

void WaveguideKernel::process (const WaveguideProcessView& view) noexcept
{
    assert (prepared);
    assert (view.numSamples >= 0 && view.numSamples <= maximumBlockSize);
    assert (view.output[0] != nullptr && view.output[1] != nullptr);
    assert (view.bankInput[0] != nullptr && view.bankInput[1] != nullptr);

    if (! prepared || view.numSamples <= 0 || view.numSamples > maximumBlockSize
        || view.output[0] == nullptr || view.output[1] == nullptr
        || view.bankInput[0] == nullptr || view.bankInput[1] == nullptr)
        return;

    // An empty or non-normalizable bank leaves its destination untouched.
    if (cache.enabledCount == 0
        || cache.totalGain[0] == 0.0f
        || cache.totalGain[1] == 0.0f)
        return;

    // Hoist the bank mix/input gain once for all lanes. The integration owns
    // these max-block buffers; the kernel never allocates transient storage.
    for (int channel = 0; channel < channelCount; ++channel)
    {
        const auto* exciter = view.exciter[static_cast<std::size_t> (channel)];
        const auto* previous = view.previousBank[static_cast<std::size_t> (channel)];
        auto* input = view.bankInput[static_cast<std::size_t> (channel)];
        for (int sample = 0; sample < view.numSamples; ++sample)
        {
            const auto exciterSample = exciter == nullptr ? 0.0f : exciter[sample];
            const auto previousSample = previous == nullptr ? 0.0f : previous[sample];
            input[sample] = (exciterSample * cache.exciterMix
                             + previousSample * cache.previousBankMix)
                            * cache.inputGain;
        }
    }

    (this->*processFunction) (view);
    refreshDynamicMasks();
}

void WaveguideKernel::processParallel (const WaveguideProcessView& view) noexcept
{
    auto* outputLeft = view.output[0];
    auto* outputRight = view.output[1];
    std::fill_n (outputLeft, view.numSamples, 0.0f);
    std::fill_n (outputRight, view.numSamples, 0.0f);

    for (int enabledIndex = 0; enabledIndex < cache.enabledCount; ++enabledIndex)
    {
        const auto resonator = static_cast<int> (cache.enabledIndices[static_cast<std::size_t> (enabledIndex)]);
        for (int channel = 0; channel < channelCount; ++channel)
        {
            const auto lane = laneIndex (resonator, channel);
            const auto& laneCache = cache.lanes[static_cast<std::size_t> (lane)];
            if (! laneCache.enabled)
                continue;

            auto* output = view.output[static_cast<std::size_t> (channel)];
            auto* solo = view.solo[static_cast<std::size_t> (channel)];
            const auto* input = view.bankInput[static_cast<std::size_t> (channel)];

            if (laneCache.passthrough)
            {
                // In parallel passthrough mode, feed the bank input directly
                // through the post filter without touching the delay latch.
                auto postState = postStates[static_cast<std::size_t> (lane)];
                for (int sample = 0; sample < view.numSamples; ++sample)
                {
                    const auto audible = processFilter (laneCache.postFilter,
                                                        postState,
                                                        input[sample])
                                          * laneCache.gain;
                    output[sample] += audible;
                    if (resonator == cache.soloResonator && solo != nullptr)
                        solo[sample] = audible;
                }
                postStates[static_cast<std::size_t> (lane)] = postState;
                continue;
            }

            const auto laneBit = static_cast<std::uint16_t> (1u << lane);
            if (((cache.coldMask | cache.retuningMask) & laneBit) != 0)
            {
                for (int sample = 0; sample < view.numSamples; ++sample)
                {
                    const auto directInput = input[sample];
                    const auto popped = popLane (lane);
                    pushLane (lane, popped + directInput);
                    const auto audible = postProcessLane (lane, popped) * laneCache.gain;
                    output[sample] += audible;
                    if (resonator == cache.soloResonator && solo != nullptr)
                        solo[sample] = audible;
                }
                continue;
            }

            // Warm stationary lane-block fast path: all recursive state and
            // cursor/tap metadata are loaded once and committed once.
            auto loopState = loopStates[static_cast<std::size_t> (lane)];
            auto postState = postStates[static_cast<std::size_t> (lane)];
            auto dispersionPrevious = dispersionStates[static_cast<std::size_t> (lane)].previous;
            delay.processStationaryLaneBlock (
                lane,
                view.numSamples,
                [&] (float delayedSample, int sample) noexcept
                {
                    const auto directInput = input[sample];
                    auto resonated = processFilter (laneCache.loopFilter, loopState, delayedSample);
                    const auto newDispersionState = laneCache.dispersionC * resonated
                                                    - laneCache.dispersionS * dispersionPrevious;
                    resonated = laneCache.dispersionS * resonated
                                + laneCache.dispersionC * dispersionPrevious;
                    dispersionPrevious = newDispersionState;
                    resonated *= laneCache.decayCoefficient;

                    const auto audible = processFilter (laneCache.postFilter, postState, resonated)
                                          * laneCache.gain;
                    output[sample] += audible;
                    if (resonator == cache.soloResonator && solo != nullptr)
                        solo[sample] = audible;
                    return resonated + directInput;
                });
            loopStates[static_cast<std::size_t> (lane)] = loopState;
            postStates[static_cast<std::size_t> (lane)] = postState;
            dispersionStates[static_cast<std::size_t> (lane)].previous = dispersionPrevious;
        }
    }

    const auto leftScale = cache.inverseTotalGain[0] * cache.outputGain;
    const auto rightScale = cache.inverseTotalGain[1] * cache.outputGain;
    for (int sample = 0; sample < view.numSamples; ++sample)
    {
        outputLeft[sample] *= leftScale;
        outputRight[sample] *= rightScale;
    }
}

void WaveguideKernel::processInterlinked (const WaveguideProcessView& view) noexcept
{
    for (int sample = 0; sample < view.numSamples; ++sample)
    {
        std::array<float, resonatorCount> poppedLeft {};
        std::array<float, resonatorCount> poppedRight {};
        float outputLeft = 0.0f;
        float outputRight = 0.0f;
        float feedbackLeft = 0.0f;
        float feedbackRight = 0.0f;

        for (int enabledIndex = 0; enabledIndex < cache.enabledCount; ++enabledIndex)
        {
            const auto resonator = static_cast<int> (cache.enabledIndices[static_cast<std::size_t> (enabledIndex)]);
            const auto leftLane = laneIndex (resonator, 0);
            const auto rightLane = laneIndex (resonator, 1);
            const auto& leftCache = cache.lanes[static_cast<std::size_t> (leftLane)];
            const auto& rightCache = cache.lanes[static_cast<std::size_t> (rightLane)];

            if (leftCache.enabled)
            {
                poppedLeft[static_cast<std::size_t> (resonator)] = popLane (leftLane);
                feedbackLeft += poppedLeft[static_cast<std::size_t> (resonator)]
                                * (leftCache.gain * cache.inverseTotalGain[0]);
                outputLeft += postProcessLane (leftLane,
                                               poppedLeft[static_cast<std::size_t> (resonator)])
                              * leftCache.gain;
            }

            if (rightCache.enabled)
            {
                poppedRight[static_cast<std::size_t> (resonator)] = popLane (rightLane);
                feedbackRight += poppedRight[static_cast<std::size_t> (resonator)]
                                 * (rightCache.gain * cache.inverseTotalGain[1]);
                // Each stereo lane feeds its own popped signal to its post filter.
                outputRight += postProcessLane (rightLane,
                                                poppedRight[static_cast<std::size_t> (resonator)])
                               * rightCache.gain;
            }
        }

        if (cache.soloResonator >= 0)
        {
            if (view.solo[0] != nullptr)
                view.solo[0][sample] = poppedLeft[static_cast<std::size_t> (cache.soloResonator)];
            if (view.solo[1] != nullptr)
                view.solo[1][sample] = poppedRight[static_cast<std::size_t> (cache.soloResonator)];
        }

        feedbackLeft *= -2.0f;
        feedbackRight *= -2.0f;
        const auto inputLeft = view.bankInput[0][sample];
        const auto inputRight = view.bankInput[1][sample];

        for (int enabledIndex = 0; enabledIndex < cache.enabledCount; ++enabledIndex)
        {
            const auto resonator = static_cast<int> (cache.enabledIndices[static_cast<std::size_t> (enabledIndex)]);
            const auto leftLane = laneIndex (resonator, 0);
            const auto rightLane = laneIndex (resonator, 1);
            if (cache.lanes[static_cast<std::size_t> (leftLane)].enabled)
                pushLane (leftLane,
                          feedbackLeft + poppedLeft[static_cast<std::size_t> (resonator)] + inputLeft);
            if (cache.lanes[static_cast<std::size_t> (rightLane)].enabled)
                pushLane (rightLane,
                          feedbackRight + poppedRight[static_cast<std::size_t> (resonator)] + inputRight);
        }

        view.output[0][sample] = outputLeft * cache.outputGain;
        view.output[1][sample] = outputRight * cache.outputGain;
    }
}

void WaveguideKernel::processCascade (const WaveguideProcessView& view) noexcept
{
    for (int sample = 0; sample < view.numSamples; ++sample)
    {
        const auto inputLeft = view.bankInput[0][sample];
        const auto inputRight = view.bankInput[1][sample];
        float previousLeft = 0.0f;
        float previousRight = 0.0f;
        float outputLeft = 0.0f;
        float outputRight = 0.0f;

        for (int enabledIndex = 0; enabledIndex < cache.enabledCount; ++enabledIndex)
        {
            const auto resonator = static_cast<int> (cache.enabledIndices[static_cast<std::size_t> (enabledIndex)]);
            const auto leftLane = laneIndex (resonator, 0);
            const auto rightLane = laneIndex (resonator, 1);
            const auto& leftCache = cache.lanes[static_cast<std::size_t> (leftLane)];
            const auto& rightCache = cache.lanes[static_cast<std::size_t> (rightLane)];

            if (leftCache.enabled)
            {
                const auto popped = popLane (leftLane);
                const auto forwarded = processCascadeFilter (
                    leftLane,
                    processCascadeDcBlocker (leftLane, previousLeft) * cache.cascadeAmount[0]);
                pushLane (leftLane, popped + forwarded + inputLeft);
                previousLeft = popped;
                const auto audible = postProcessLane (leftLane, popped) * leftCache.gain;
                outputLeft += audible;
                if (resonator == cache.soloResonator && view.solo[0] != nullptr)
                    view.solo[0][sample] = audible;
            }

            if (rightCache.enabled)
            {
                const auto popped = popLane (rightLane);
                const auto forwarded = processCascadeFilter (
                    rightLane,
                    processCascadeDcBlocker (rightLane, previousRight) * cache.cascadeAmount[1]);
                pushLane (rightLane, popped + forwarded + inputRight);
                previousRight = popped;
                const auto audible = postProcessLane (rightLane, popped) * rightCache.gain;
                outputRight += audible;
                if (resonator == cache.soloResonator && view.solo[1] != nullptr)
                    view.solo[1][sample] = audible;
            }
        }

        view.output[0][sample] = outputLeft * cache.outputGain;
        view.output[1][sample] = outputRight * cache.outputGain;
    }
}

float WaveguideKernel::popLane (int lane) noexcept
{
    auto& laneCache = cache.lanes[static_cast<std::size_t> (lane)];
    if (laneCache.passthrough)
        return passthroughSamples[static_cast<std::size_t> (lane)];

    auto sample = delay.popSample (lane);
    sample = processFilter (laneCache.loopFilter,
                            loopStates[static_cast<std::size_t> (lane)],
                            sample);

    auto& dispersionState = dispersionStates[static_cast<std::size_t> (lane)];
    const auto newState = laneCache.dispersionC * sample
                          - laneCache.dispersionS * dispersionState.previous;
    const auto output = laneCache.dispersionS * sample
                        + laneCache.dispersionC * dispersionState.previous;
    dispersionState.previous = newState;
    return output * laneCache.decayCoefficient;
}

void WaveguideKernel::pushLane (int lane, float sample) noexcept
{
    if (cache.lanes[static_cast<std::size_t> (lane)].passthrough)
    {
        passthroughSamples[static_cast<std::size_t> (lane)] = sample;
        return;
    }

    delay.pushSample (lane, sample);
}

float WaveguideKernel::postProcessLane (int lane, float sample) noexcept
{
    return processFilter (cache.lanes[static_cast<std::size_t> (lane)].postFilter,
                          postStates[static_cast<std::size_t> (lane)],
                          sample);
}

float WaveguideKernel::processCascadeFilter (int lane, float sample) noexcept
{
    const auto channel = lane % channelCount;
    return processFilter (cache.cascadeFilter[static_cast<std::size_t> (channel)],
                          cascadeStates[static_cast<std::size_t> (lane)],
                          sample);
}

float WaveguideKernel::processCascadeDcBlocker (int lane, float sample) noexcept
{
    const auto index = static_cast<std::size_t> (lane);
    const auto output = sample - cascadeDcInputHistory[index]
                        + cascadeDcPole * cascadeDcOutputHistory[index];
    cascadeDcInputHistory[index] = sample;
    cascadeDcOutputHistory[index] = output;
    return output;
}

void WaveguideKernel::refreshDynamicMasks() noexcept
{
    cache.retuningMask = delay.getRetuningMask() & cache.enabledLaneMask
                         & static_cast<std::uint16_t> (~cache.passthroughMask);
    cache.coldMask = delay.getColdMask() & cache.enabledLaneMask
                     & static_cast<std::uint16_t> (~cache.passthroughMask);
}
} // namespace resonarium::waveguide
