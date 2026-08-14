#include "PluginProcessor.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <memory>

namespace
{
constexpr int sampleRate = 48'000;
constexpr int blockSize = 128;
constexpr int renderedBlocks = 32;

struct RenderResult
{
    std::array<float, renderedBlocks * blockSize * 2> samples {};
    double leftEnergy = 0.0;
    double rightEnergy = 0.0;
    bool stereoBusesConfigured = false;
    bool finite = true;
    ExternalInputFrame::Telemetry inputTelemetry;
};

struct OffsetResult
{
    float preEventMagnitude = 0.0f;
    float postEventLeftMagnitude = 0.0f;
    float postEventRightMagnitude = 0.0f;
    ExternalInputFrame::Telemetry inputTelemetry;
    bool finite = true;
};

struct DryWetOracleResult
{
    float mix = 0.0f;
    float gainDb = 0.0f;
    float maximumError = 0.0f;
    bool finite = true;
    bool asymmetric = false;
    bool inputCapturedOnce = false;
};

void setParameter(gin::Parameter& parameter, float value)
{
    parameter.setUserValue(value);
}

RenderResult renderEffect(bool routeAllBanks)
{
    auto processor = std::make_unique<ResonariumProcessor>();
    RenderResult result;
    auto layout = processor->getBusesLayout();
    if (layout.inputBuses.isEmpty() || layout.outputBuses.isEmpty())
    {
        result.finite = false;
        return result;
    }

    layout.inputBuses.set(0, juce::AudioChannelSet::stereo());
    layout.outputBuses.set(0, juce::AudioChannelSet::stereo());
    result.stereoBusesConfigured = processor->setBusesLayout(layout)
                                     && processor->getMainBusNumInputChannels() == 2
                                     && processor->getMainBusNumOutputChannels() == 2;
    if (!result.stereoBusesConfigured)
    {
        result.finite = false;
        return result;
    }

    processor->prepareToPlay(sampleRate, blockSize);

    auto& voice = processor->synth.params.voiceParams;
    auto& external = voice.externalInputExciterParams;
    setParameter(*external.enabled, 1.0f);
    setParameter(*external.mix, 1.0f);
    setParameter(*external.constantNote, 1.0f);
    for (int bankIndex = 0; bankIndex < NUM_RESONATOR_BANKS; ++bankIndex)
    {
        auto& bank = voice.waveguideResonatorBankParams[static_cast<std::size_t>(bankIndex)];
        const auto resonatorIndex = static_cast<std::size_t>(bankIndex % NUM_RESONATORS);
        setParameter(*bank.resonatorParams[resonatorIndex].enabled,
                     routeAllBanks || bankIndex == 0 ? 1.0f : 0.0f);
        setParameter(*bank.resonatorParams[resonatorIndex].gain, 1.0f);
        setParameter(*bank.couplingMode, static_cast<float>(bankIndex % 3));
        setParameter(*bank.inputMix, bankIndex == 0 ? 0.0f : 1.0f);
        setParameter(*bank.inputGain, 1.0f);
        setParameter(*bank.outputGain, 1.0f);
    }

    juce::AudioBuffer<float> audio(2, blockSize);
    juce::MidiBuffer midi;
    for (int block = 0; block < renderedBlocks; ++block)
    {
        auto* left = audio.getWritePointer(0);
        auto* right = audio.getWritePointer(1);
        for (int sample = 0; sample < blockSize; ++sample)
        {
            const auto phase = static_cast<float>(block * blockSize + sample) / 19.0f;
            left[sample] = 0.25f * std::sin(phase);
            right[sample] = 0.0f;
        }

        processor->processBlock(audio, midi);
        for (int sample = 0; sample < blockSize; ++sample)
        {
            const auto leftSample = audio.getSample(0, sample);
            const auto rightSample = audio.getSample(1, sample);
            const auto outputOffset = static_cast<std::size_t>((block * blockSize + sample) * 2);
            result.samples[outputOffset] = leftSample;
            result.samples[outputOffset + 1] = rightSample;
            result.finite = result.finite && std::isfinite(leftSample) && std::isfinite(rightSample);
            result.leftEnergy += static_cast<double>(leftSample) * leftSample;
            result.rightEnergy += static_cast<double>(rightSample) * rightSample;
        }
    }
    result.inputTelemetry = processor->synth.getExternalInputFrame().getTelemetry();
    return result;
}

OffsetResult renderOffsetInput(int noteOffset)
{
    auto processor = std::make_unique<ResonariumProcessor>();
    OffsetResult result;
    auto layout = processor->getBusesLayout();
    layout.inputBuses.set(0, juce::AudioChannelSet::stereo());
    layout.outputBuses.set(0, juce::AudioChannelSet::stereo());
    if (!processor->setBusesLayout(layout))
    {
        result.finite = false;
        return result;
    }

    processor->prepareToPlay(sampleRate, blockSize);
    auto& external = processor->synth.params.voiceParams.externalInputExciterParams;
    setParameter(*external.enabled, 1.0f);
    setParameter(*external.mix, 1.0f);
    setParameter(*external.gain, 0.0f);
    setParameter(*external.adsrParams.attack, 0.0f);
    setParameter(*external.adsrParams.decay, 0.0f);
    setParameter(*external.adsrParams.sustain, 100.0f);
    setParameter(*external.adsrParams.release, 0.0f);
    setParameter(*processor->uiParams.bypassResonators, 1.0f);

    juce::AudioBuffer<float> audio(2, blockSize);
    for (int sample = 0; sample < blockSize; ++sample)
    {
        audio.setSample(0, sample, 0.2f + 0.001f * static_cast<float>(sample));
        audio.setSample(1, sample, -0.1f - 0.0005f * static_cast<float>(sample));
    }
    juce::MidiBuffer midi;
    midi.addEvent(juce::MidiMessage::noteOn(2, 60, static_cast<juce::uint8>(100)), noteOffset);
    processor->processBlock(audio, midi);

    for (int sample = 0; sample < blockSize; ++sample)
    {
        const auto left = audio.getSample(0, sample);
        const auto right = audio.getSample(1, sample);
        result.finite = result.finite && std::isfinite(left) && std::isfinite(right);
        if (sample < noteOffset)
            result.preEventMagnitude = std::max(result.preEventMagnitude,
                                                std::max(std::abs(left), std::abs(right)));
        else
        {
            result.postEventLeftMagnitude = std::max(result.postEventLeftMagnitude, std::abs(left));
            result.postEventRightMagnitude = std::max(result.postEventRightMagnitude, std::abs(right));
        }
    }
    result.inputTelemetry = processor->synth.getExternalInputFrame().getTelemetry();
    return result;
}

DryWetOracleResult renderDryWetOracle(float mix, float gainDb)
{
    auto processor = std::make_unique<ResonariumProcessor>();
    DryWetOracleResult result { mix, gainDb };
    auto layout = processor->getBusesLayout();
    layout.inputBuses.set(0, juce::AudioChannelSet::stereo());
    layout.outputBuses.set(0, juce::AudioChannelSet::stereo());
    if (!processor->setBusesLayout(layout))
    {
        result.finite = false;
        return result;
    }

    processor->prepareToPlay(sampleRate, blockSize);
    auto& external = processor->synth.params.voiceParams.externalInputExciterParams;
    setParameter(*external.enabled, 1.0f);
    setParameter(*external.mix, mix);
    setParameter(*external.gain, gainDb);
    setParameter(*external.adsrParams.attack, 0.0f);
    setParameter(*external.adsrParams.decay, 0.0f);
    setParameter(*external.adsrParams.sustain, 100.0f);
    setParameter(*external.adsrParams.release, 0.0f);
    setParameter(*processor->synth.params.voiceParams.masterGain, 0.0f);
    setParameter(*processor->uiParams.bypassResonators, 1.0f);

    constexpr int eventOffset = 0;
    std::array<float, blockSize> inputLeft {};
    std::array<float, blockSize> inputRight {};
    juce::AudioBuffer<float> audio(2, blockSize);
    for (int sample = 0; sample < blockSize; ++sample)
    {
        inputLeft[static_cast<std::size_t>(sample)] = 0.11f + 0.0007f * static_cast<float>(sample);
        inputRight[static_cast<std::size_t>(sample)] = -0.07f - 0.0003f * static_cast<float>(sample);
        audio.setSample(0, sample, inputLeft[static_cast<std::size_t>(sample)]);
        audio.setSample(1, sample, inputRight[static_cast<std::size_t>(sample)]);
    }
    juce::MidiBuffer midi;
    midi.addEvent(juce::MidiMessage::noteOn(2, 60, static_cast<juce::uint8>(127)), eventOffset);
    processor->processBlock(audio, midi);

    const auto dryGain = std::cos(mix * juce::MathConstants<float>::halfPi);
    const auto wetGain = std::sin(mix * juce::MathConstants<float>::halfPi)
                         * juce::Decibels::decibelsToGain(gainDb);
    constexpr float sourceTrim = 0.05f;
    float leftEnergy = 0.0f;
    float rightEnergy = 0.0f;
    // AnalogADSR has a historical first-sample transition. Samples after the
    // onset are stationary at unity sustain, so the exact routing law applies.
    for (int sample = eventOffset + 1; sample < blockSize; ++sample)
    {
        const auto expectedLeft = inputLeft[static_cast<std::size_t>(sample)]
                                  * (dryGain + sourceTrim * wetGain);
        const auto expectedRight = inputRight[static_cast<std::size_t>(sample)]
                                   * (dryGain + sourceTrim * wetGain);
        const auto actualLeft = audio.getSample(0, sample);
        const auto actualRight = audio.getSample(1, sample);
        result.finite = result.finite && std::isfinite(actualLeft) && std::isfinite(actualRight);
        result.maximumError = std::max(
            result.maximumError,
            std::max(std::abs(actualLeft - expectedLeft), std::abs(actualRight - expectedRight)));
        leftEnergy += actualLeft * actualLeft;
        rightEnergy += actualRight * actualRight;
    }
    result.asymmetric = std::abs(leftEnergy - rightEnergy)
                        > std::max(1.0e-20f, (leftEnergy + rightEnergy) * 1.0e-6f);
    const auto telemetry = processor->synth.getExternalInputFrame().getTelemetry();
    result.inputCapturedOnce = telemetry.hostBlocks == 1 && telemetry.validBlocks == 1
                               && telemetry.sourceSamplesCopied
                                      == static_cast<std::uint64_t>(blockSize * 2);
    return result;
}
} // namespace

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;
    const auto fullChain = renderEffect(true);
    const auto bankZeroOnly = renderEffect(false);
    constexpr int noteOffset = 37;
    const auto offsetInput = renderOffsetInput(noteOffset);
    constexpr std::array<float, 3> dryWetMixes { 0.0f, 0.5f, 1.0f };
    constexpr std::array<float, 3> dryWetGains { -100.0f, 0.0f, 24.0f };
    std::array<DryWetOracleResult, dryWetMixes.size() * dryWetGains.size()> dryWetResults {};
    std::size_t dryWetIndex = 0;
    bool dryWetExact = true;
    for (const auto mix : dryWetMixes)
        for (const auto gain : dryWetGains)
        {
            auto& result = dryWetResults[dryWetIndex++];
            result = renderDryWetOracle(mix, gain);
            dryWetExact = dryWetExact && result.finite && result.asymmetric
                          && result.inputCapturedOnce && result.maximumError <= 2.0e-6f;
        }
    float routedDifference = 0.0f;
    for (std::size_t index = 0; index < fullChain.samples.size(); ++index)
        routedDifference = std::max(
            routedDifference, std::abs(fullChain.samples[index] - bankZeroOnly.samples[index]));

    const bool audible = fullChain.leftEnergy + fullChain.rightEnergy > 1.0e-10;
    const bool asymmetric = std::abs(fullChain.leftEnergy - fullChain.rightEnergy) > 1.0e-12;
    const bool downstreamBanksContributed = routedDifference > 1.0e-7f;
    const bool inputCapturedOncePerBlock =
        fullChain.inputTelemetry.hostBlocks == renderedBlocks
        && fullChain.inputTelemetry.validBlocks == renderedBlocks
        && fullChain.inputTelemetry.sourceSamplesCopied
               == static_cast<std::uint64_t>(renderedBlocks * blockSize * 2);
    const bool inputOffsetCorrect = offsetInput.finite
                                    && offsetInput.preEventMagnitude < 1.0e-6f
                                    && offsetInput.postEventLeftMagnitude > 1.0e-5f
                                    && offsetInput.postEventRightMagnitude > 1.0e-5f
                                    && offsetInput.inputTelemetry.hostBlocks == 1
                                    && offsetInput.inputTelemetry.validBlocks == 1
                                    && offsetInput.inputTelemetry.sourceSamplesCopied
                                           == static_cast<std::uint64_t>(blockSize * 2);

    std::fprintf(stderr,
                 "effect external-input gate: stereo_buses=%s finite=%s audible=%s asymmetric=%s "
                 "routed=%s input_once=%s offset=%s left_energy=%.9g right_energy=%.9g "
                 "max_routed_delta=%.9g input_blocks=%llu input_samples=%llu pre=%.9g post_l=%.9g post_r=%.9g\n",
                 fullChain.stereoBusesConfigured ? "true" : "false",
                 fullChain.finite ? "true" : "false", audible ? "true" : "false",
                 asymmetric ? "true" : "false",
                 downstreamBanksContributed ? "true" : "false",
                 inputCapturedOncePerBlock ? "true" : "false",
                 inputOffsetCorrect ? "true" : "false",
                 fullChain.leftEnergy, fullChain.rightEnergy, routedDifference,
                 static_cast<unsigned long long>(fullChain.inputTelemetry.hostBlocks),
                 static_cast<unsigned long long>(fullChain.inputTelemetry.sourceSamplesCopied),
                 offsetInput.preEventMagnitude, offsetInput.postEventLeftMagnitude,
                 offsetInput.postEventRightMagnitude);
    for (const auto& result : dryWetResults)
        std::fprintf(stderr,
                     "effect dry/wet oracle: mix=%.2f gain_db=%.1f finite=%s asymmetric=%s "
                     "input_once=%s max_error=%.9g\n",
                     result.mix, result.gainDb, result.finite ? "true" : "false",
                     result.asymmetric ? "true" : "false",
                     result.inputCapturedOnce ? "true" : "false", result.maximumError);
    return fullChain.stereoBusesConfigured && bankZeroOnly.stereoBusesConfigured
                   && fullChain.finite && bankZeroOnly.finite && audible && asymmetric
                   && downstreamBanksContributed && inputCapturedOncePerBlock
                   && inputOffsetCorrect && dryWetExact
               ? 0
               : 1;
}
