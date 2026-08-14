/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Scalar multimode TPT state-variable filter adapted for Resonarium from
 * Gabriel Soule's Applause implementation
 * (applause/dsp/filters/StateVariableFilter.h at f1098bf2). Applause describes
 * its source as MIT-licensed, which is compatible with this GPLv3 port. The
 * design follows Andrew Simper's Cytomic SVF work and the ChowDSP implementation
 * derived from it. Applause is not a build or runtime dependency of Resonarium.
 */

#pragma once

#include <juce_dsp/juce_dsp.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <concepts>
#include <cstddef>
#include <limits>
#include <numbers>

namespace resonarium
{
/**
 * A scalar second-order multimode state-variable filter, digitized using
 * trapezoidal integration.
 *
 * Mode 0 is lowpass, 0.5 is bandpass, and 1 is highpass. Intermediate modes
 * use the linear blend historically used by Resonarium's ChowDSP fork.
 */
template <std::floating_point SampleType = float, std::size_t MaxChannels = 2, bool UnityGain = false>
class StateVariableFilter
{
public:
    static_assert (MaxChannels >= 1, "The filter needs at least one channel of state");

    static constexpr SampleType inverseRootTwo = static_cast<SampleType> (0.70710678118654752440);
    static constexpr SampleType minimumResonance = static_cast<SampleType> (0.01);

    StateVariableFilter()
    {
        reset();
        setMode (SampleType { 0 });
        setCutoffFrequency (SampleType { 1000 });
        setQValue (inverseRootTwo);
    }

    /** Sets the sample rate, updates the coefficients, and clears the state. */
    void prepare (const juce::dsp::ProcessSpec& spec)
    {
        jassert (spec.sampleRate > 0.0);
        jassert (spec.numChannels > 0);
        jassert (spec.numChannels <= MaxChannels);

        sampleRate = spec.sampleRate;
        reset();

        // This deliberately treats the stored frequency as a cutoff, matching
        // the legacy filter when prepare() follows a peak-frequency update.
        setCutoffFrequency (cutoffFrequency);
    }

    void reset() noexcept
    {
        state1.fill (SampleType { 0 });
        state2.fill (SampleType { 0 });
    }

    /** Flushes denormal state after sample-by-sample processing. */
    void snapToZero() noexcept
    {
#if JUCE_SNAP_TO_ZERO
        for (auto& value : state1)
            juce::dsp::util::snapToZero (value);
        for (auto& value : state2)
            juce::dsp::util::snapToZero (value);
#endif
    }

    template <bool ShouldUpdate = true>
    void setCutoffFrequency (SampleType newFrequency)
    {
        newFrequency = clampFrequencyBelowNyquist (newFrequency);

        cutoffFrequency = newFrequency;
        const auto radians = std::numbers::pi_v<SampleType> * cutoffFrequency
                             / static_cast<SampleType> (sampleRate);
        g = std::tan (radians);

        if constexpr (ShouldUpdate)
            update();
    }

    /**
     * Interprets the supplied frequency as the resonant peak. This intentionally
     * preserves Resonarium's legacy conversion for every multimode position.
     */
    template <bool ShouldUpdate = true>
    void setPeakFrequency (SampleType newFrequency)
    {
        newFrequency = clampFrequencyBelowNyquist (newFrequency);

        cutoffFrequency = newFrequency;
        const auto radians = std::numbers::pi_v<SampleType> * newFrequency
                             / static_cast<SampleType> (sampleRate);
        const auto denominatorSquared = std::max (
            resonance * resonance - SampleType { 0.5 },
            std::numeric_limits<SampleType>::min());
        const auto denominator = std::sqrt (denominatorSquared);
        g = resonance * std::tan (radians) / denominator;
        jassert (! std::isnan (g));

        if constexpr (ShouldUpdate)
            update();
    }

    template <bool ShouldUpdate = true>
    void setQValue (SampleType newResonance)
    {
        resonance = sanitizeResonance (newResonance);
        k = SampleType { 1 } / resonance;

        if constexpr (ShouldUpdate)
            update();
    }

    /**
     * Sets the linear lowpass/bandpass/highpass blend. Direct calls do not
     * update unity normalization; call update() after deferred parameter edits.
     */
    void setMode (SampleType newMode)
    {
        jassert (newMode >= SampleType { 0 } && newMode <= SampleType { 1 });

        if constexpr (UnityGain)
        {
            const auto isDiscreteMode = newMode == SampleType { 0 }
                                        || newMode == SampleType { 0.5 }
                                        || newMode == SampleType { 1 };
            jassert (isDiscreteMode);
            (void) isDiscreteMode;
        }

        mode = newMode;
        lowpassMix = SampleType { 1 } - SampleType { 2 } * std::min (SampleType { 0.5 }, mode);
        bandpassMix = SampleType { 1 } - std::abs (SampleType { 2 } * (mode - SampleType { 0.5 }));
        highpassMix = SampleType { 2 } * std::max (SampleType { 0.5 }, mode) - SampleType { 1 };
    }

    /** Recomputes all coefficients after deferred setter calls. */
    void update() noexcept
    {
        const auto gPlusK = g + k;
        gt0 = SampleType { 1 } / (SampleType { 1 } + g * gPlusK);
        gk0 = gPlusK * gt0;
        gt1 = g * gt0;
        gk1 = g * gk0;
        gt2 = g * gt1;

        if constexpr (UnityGain)
            oneOverPeakGain = SampleType { 1 } / getPeakGain();
    }

    /**
     * Updates mode, resonance, and frequency together and reports whether any
     * stored parameter changed. The ordering and comparisons match the legacy
     * Resonarium filter.
     */
    bool updateParameters (SampleType newFrequency,
                           SampleType newResonance,
                           SampleType newMode,
                           bool frequencyIsPeak = false)
    {
        const auto effectiveFrequency = clampFrequencyBelowNyquist (newFrequency);
        const auto effectiveResonance = sanitizeResonance (newResonance);
        bool changed = false;

        if (newMode != mode)
        {
            setMode (newMode);
            changed = true;
        }

        if (frequencyIsPeak)
        {
            bool resonanceChanged = false;
            if (effectiveResonance != resonance)
            {
                setQValue<false> (effectiveResonance);
                resonanceChanged = true;
                changed = true;
            }

            if (effectiveFrequency != cutoffFrequency || resonanceChanged)
            {
                setPeakFrequency<false> (effectiveFrequency);
                changed = true;
            }
        }
        else
        {
            if (effectiveResonance != resonance)
            {
                setQValue<false> (effectiveResonance);
                changed = true;
            }

            if (effectiveFrequency != cutoffFrequency)
            {
                setCutoffFrequency<false> (effectiveFrequency);
                changed = true;
            }
        }

        if (changed)
            update();

        return changed;
    }

    [[nodiscard]] SampleType getCutoffFrequency() const noexcept { return cutoffFrequency; }
    [[nodiscard]] SampleType getQValue() const noexcept { return resonance; }
    [[nodiscard]] SampleType getResonance() const noexcept { return resonance; }

    /** Returns the unnormalised resonant peak gain. */
    [[nodiscard]] SampleType getPeakGain() const noexcept
    {
        if (lowpassMix >= SampleType { 0.98 } || highpassMix >= SampleType { 0.98 })
        {
            if (resonance > inverseRootTwo)
            {
                const auto kSquared = k * k;
                return SampleType { 2 }
                       / (kSquared * std::sqrt (SampleType { 4 } / kSquared - SampleType { 1 }));
            }

            return SampleType { 1 };
        }

        if (bandpassMix == SampleType { 1 })
            return resonance;

        if (resonance < inverseRootTwo)
            return lowpassMix + highpassMix;

        // Retain the legacy linear-blend peak calculation. UnityGain instances
        // are asserted to discrete modes, so this branch is only informational
        // for the supported unity-normalised configurations.
        const auto a = static_cast<double> (lowpassMix == SampleType { 0 } ? highpassMix : lowpassMix);
        const auto b = static_cast<double> (bandpassMix);
        const auto q = static_cast<double> (resonance);
        const auto qSquared = q * q;
        const auto aSquared = a * a;
        const auto bSquared = b * b;
        const auto term1 = std::sqrt (-aSquared * bSquared
                                      + (aSquared + bSquared) * (aSquared + bSquared) * qSquared);
        const auto term2 = bSquared * (1.0 - 2.0 * qSquared)
                           + 2.0 * q * (-aSquared * q + term1);
        const auto result = bSquared * q * std::sqrt (1.0 / term2);
        jassert (! std::isnan (result));
        return static_cast<SampleType> (result);
    }

    /** Processes one sample using channel zero's state. */
    [[nodiscard]] SampleType processSample (SampleType input) noexcept
    {
        return processSampleInternal (input, state1[0], state2[0]);
    }

    /** Processes one sample using the selected independent channel state. */
    [[nodiscard]] SampleType processSample (std::size_t channel, SampleType input) noexcept
    {
        jassert (channel < MaxChannels);
        return processSampleInternal (input, state1[channel], state2[channel]);
    }

    /** Processes JUCE replacing and non-replacing contexts. */
    template <typename ProcessContext>
    void process (const ProcessContext& context) noexcept
    {
        const auto& inputBlock = context.getInputBlock();
        auto& outputBlock = context.getOutputBlock();
        const auto numChannels = outputBlock.getNumChannels();
        const auto numSamples = outputBlock.getNumSamples();

        jassert (inputBlock.getNumChannels() <= MaxChannels);
        jassert (inputBlock.getNumChannels() == numChannels);
        jassert (inputBlock.getNumSamples() == numSamples);

        if (context.isBypassed)
        {
            outputBlock.copyFrom (inputBlock);
            return;
        }

        for (std::size_t channel = 0; channel < numChannels; ++channel)
        {
            const auto* input = inputBlock.getChannelPointer (channel);
            auto* output = outputBlock.getChannelPointer (channel);
            auto s1 = state1[channel];
            auto s2 = state2[channel];

            for (std::size_t sample = 0; sample < numSamples; ++sample)
                output[sample] = processSampleInternal (input[sample], s1, s2);

            state1[channel] = s1;
            state2[channel] = s2;
        }

#if JUCE_SNAP_TO_ZERO
        snapToZero();
#endif
    }

    /** Returns the phase response at a frequency in radians. */
    [[nodiscard]] SampleType getPhaseResponse (SampleType frequency) const noexcept
    {
        const auto omega = SampleType { 2 } * std::numbers::pi_v<SampleType> * frequency
                           / static_cast<SampleType> (sampleRate);
        const std::complex<SampleType> z = std::exp (std::complex<SampleType> { SampleType { 0 }, omega });

        const auto gSquared = g * g;
        const auto gTimesK = g * k;
        const auto onePlusZ = SampleType { 1 } + z;
        const auto zMinusOne = z - SampleType { 1 };
        const auto zSquaredMinusOne = z * z - SampleType { 1 };

        const auto lowpassNumerator = gSquared * onePlusZ * onePlusZ;
        const auto bandpassNumerator = g * zSquaredMinusOne;
        const auto highpassNumerator = zMinusOne * zMinusOne;
        const auto numerator = lowpassMix * lowpassNumerator
                               + bandpassMix * bandpassNumerator
                               + highpassMix * highpassNumerator;
        const auto denominator = zMinusOne * zMinusOne
                                 + gSquared * onePlusZ * onePlusZ
                                 + gTimesK * zSquaredMinusOne;

        return std::arg (numerator / denominator);
    }

    /** Returns phase delay at a frequency, measured in samples. */
    [[nodiscard]] SampleType getPhaseDelayInSamples (SampleType frequency) const noexcept
    {
        if (frequency <= SampleType { 0 })
            return SampleType { 0 };

        const auto omega = SampleType { 2 } * std::numbers::pi_v<SampleType> * frequency
                           / static_cast<SampleType> (sampleRate);
        return -getPhaseResponse (frequency) / omega;
    }

private:
    /**
     * Preserve the filter's original ChowDSP low-rate behaviour: host-visible
     * controls may reach 20 kHz, but the TPT integrator must never receive a
     * frequency at or above the current Nyquist limit. One hertz of headroom
     * also avoids evaluating tan() directly at pi/2.
     */
    [[nodiscard]] SampleType clampFrequencyBelowNyquist (SampleType frequency) const noexcept
    {
        const auto nyquist = static_cast<SampleType> (sampleRate * 0.5);
        const auto maximumFrequency = std::max (SampleType { 0 }, nyquist - SampleType { 1 });
        if (! std::isfinite (frequency))
            return SampleType { 0 };
        return std::clamp (frequency, SampleType { 0 }, maximumFrequency);
    }

    [[nodiscard]] static SampleType sanitizeResonance (SampleType newResonance) noexcept
    {
        if (! std::isfinite (newResonance))
            return minimumResonance;
        return std::max (newResonance, minimumResonance);
    }

    [[nodiscard]] inline SampleType processSampleInternal (SampleType input,
                                                           SampleType& s1,
                                                           SampleType& s2) noexcept
    {
        // Simper's tick-parallel form shortens the loop-carried dependency
        // chain while remaining algebraically equivalent to the legacy core.
        const auto t0 = input - s2;
        const auto t1 = gt1 * t0 - gk1 * s1;
        const auto t2 = gt2 * t0 + gt1 * s1;

        const auto highpass = gt0 * t0 - gk0 * s1;
        const auto bandpass = t1 + s1;
        const auto lowpass = t2 + s2;
        const auto output = std::fma (lowpassMix,
                                      lowpass,
                                      std::fma (bandpassMix, bandpass, highpassMix * highpass));

        s1 += SampleType { 2 } * t1;
        s2 += SampleType { 2 } * t2;

        if constexpr (UnityGain)
            return output * oneOverPeakGain;
        return output;
    }

    SampleType cutoffFrequency { 1000 };
    SampleType resonance { inverseRootTwo };
    SampleType k { SampleType { 1 } / resonance };
    SampleType g { 0 };

    // Premultiplied tick-parallel coefficients.
    SampleType gt0 { 1 };
    SampleType gk0 { k };
    SampleType gt1 { 0 };
    SampleType gk1 { 0 };
    SampleType gt2 { 0 };

    std::array<SampleType, MaxChannels> state1 {};
    std::array<SampleType, MaxChannels> state2 {};

    SampleType lowpassMix { 1 };
    SampleType bandpassMix { 0 };
    SampleType highpassMix { 0 };
    SampleType oneOverPeakGain { 1 };
    SampleType mode { 0 };
    double sampleRate { 44100.0 };
};
} // namespace resonarium
