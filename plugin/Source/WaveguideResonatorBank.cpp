#include "WaveguideResonatorBank.h"
#include "ResonatorVoice.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

void WaveguideScratchArena::prepare(const juce::dsp::ProcessSpec& spec)
{
    maximumBlockSamples = static_cast<int>(spec.maximumBlockSize);
    jassert(static_cast<int>(spec.numChannels) >= channelCount);
    jassert(maximumBlockSamples > 0);

    if (static_cast<int>(spec.numChannels) < channelCount || maximumBlockSamples <= 0)
    {
        storage.clear();
        regionChannels = {};
        maximumBlockSamples = 0;
        channelStrideSamples = 0;
        prepared = false;
        return;
    }

    constexpr int alignmentSamples = static_cast<int>(storageAlignment / sizeof(float));
    static_assert(storageAlignment % sizeof(float) == 0);
    static_assert((alignmentSamples & (alignmentSamples - 1)) == 0);
    channelStrideSamples = (maximumBlockSamples + alignmentSamples - 1) & ~(alignmentSamples - 1);
    storage.assign(static_cast<std::size_t>(regionCount * channelCount * channelStrideSamples), 0.0f);

    for (int region = 0; region < regionCount; ++region)
    {
        for (int channel = 0; channel < channelCount; ++channel)
        {
            const auto offset = static_cast<std::size_t>((region * channelCount + channel)
                                                         * channelStrideSamples);
            regionChannels[static_cast<std::size_t>(region)][static_cast<std::size_t>(channel)] =
                storage.data() + offset;
        }
    }

    prepared = true;
    jassert(isStorageAligned());
}

bool WaveguideScratchArena::isStorageAligned() const noexcept
{
    if (!prepared || storage.empty())
        return false;

    for (const auto& region : regionChannels)
    {
        for (const auto* channel : region)
        {
            if (channel == nullptr
                || reinterpret_cast<std::uintptr_t>(channel) % storageAlignment != 0)
                return false;
        }
    }
    return true;
}

juce::dsp::AudioBlock<float> WaveguideScratchArena::getBlock(Region region, int numSamples) noexcept
{
    jassert(prepared);
    jassert(numSamples >= 0 && numSamples <= maximumBlockSamples);
    auto& channels = regionChannels[regionIndex(region)];
    return { channels.data(), channels.size(), static_cast<std::size_t>(numSamples) };
}

WaveguideResonatorBank::WaveguideResonatorBank(GlobalState& state, ResonatorVoice& parentVoice,
                                               WaveguideResonatorBankParams params) :
    state(state),
    voice(parentVoice),
    params(params),
    index(params.index)
{
}

void WaveguideResonatorBank::processRouted(juce::dsp::AudioBlock<float>& exciterBlock,
                                          juce::dsp::AudioBlock<float>& previousResonatorBankBlock,
                                          juce::dsp::AudioBlock<float>& outputBlock)
{
    jassert(exciterBlock.getNumSamples() == previousResonatorBankBlock.getNumSamples());
    jassert(previousResonatorBankBlock.getNumSamples() == outputBlock.getNumSamples());
    jassert(exciterBlock.getNumChannels() == previousResonatorBankBlock.getNumChannels());
    jassert(previousResonatorBankBlock.getNumChannels() == outputBlock.getNumChannels());
    processKernel(exciterBlock, previousResonatorBankBlock, outputBlock);
}

bool WaveguideResonatorBank::isEmptyForRouting() const noexcept
{
    const auto& cache = kernel.getControlCache();
    return cache.enabledCount == 0 || cache.totalGain[0] == 0.0f || cache.totalGain[1] == 0.0f;
}

void WaveguideResonatorBank::reset()
{
    kernel.reset();
}

void WaveguideResonatorBank::prepare(const juce::dsp::ProcessSpec& spec)
{
    const auto prepared = kernel.prepare(spec.sampleRate, static_cast<int>(spec.maximumBlockSize));
    jassert(prepared);
    if (!prepared)
        return;

    updateControlFrame(440.0f, 0);
    kernel.reset();
}

void WaveguideResonatorBank::updateParameters(float newFrequency, int numSamples)
{
    couplingMode = static_cast<CouplingMode>(voice.getValue(params.couplingMode));
    inputMix = voice.getValue(params.inputMix);
    inputGain = voice.getValue(params.inputGain);
    outputGain = voice.getValue(params.outputGain);
    updateControlFrame(newFrequency, numSamples);
}

void WaveguideResonatorBank::updateControlFrame(float newFrequency, int numSamples) noexcept
{
    using namespace resonarium::waveguide;

    switch (couplingMode)
    {
        case PARALLEL:
            controlFrame.couplingMode = WaveguideCouplingMode::parallel;
            break;
        case INTERLINKED:
            controlFrame.couplingMode = WaveguideCouplingMode::interlinked;
            break;
        case CASCADE:
            controlFrame.couplingMode = WaveguideCouplingMode::cascade;
            break;
        default:
            jassertfalse;
            controlFrame.couplingMode = WaveguideCouplingMode::parallel;
            break;
    }

    controlFrame.inputMix = inputMix;
    controlFrame.inputGain = inputGain;
    controlFrame.outputGain = outputGain;
    controlFrame.soloResonator = state.soloActive && state.soloBankIndex == index
                                     ? state.soloResonatorIndex
                                     : -1;

    for (int channel = 0; channel < WaveguideControlFrame::channelCount; ++channel)
    {
        controlFrame.cascadeAmount[static_cast<std::size_t>(channel)] =
            voice.getValue(params.cascadeLevel, channel);
        auto& filter = controlFrame.cascadeFilter[static_cast<std::size_t>(channel)];
        filter.frequencyHz = voice.getValue(params.cascadeFilterCutoff, channel);
        filter.resonance = voice.getValue(params.cascadeFilterResonance, channel);
        filter.mode = voice.getValue(params.cascadeFilterMode, channel);
        filter.frequencyIsPeak = false;
    }

    constexpr float semitonesToOctaves = 1.0f / 12.0f;
    constexpr float minimumFrequency = WaveguideKernel::minimumSupportedFrequencyHz;

    for (int resonator = 0; resonator < WaveguideControlFrame::resonatorCount; ++resonator)
    {
        const auto& resonatorParams = params.resonatorParams[static_cast<std::size_t>(resonator)];
        const bool enabled = resonatorParams.enabled->isOn();
        const float gain = voice.getValue(resonatorParams.gain);

        const bool resonatorKeytrack = resonatorParams.resonatorKeytrack->isOn();
        const float resonatorPitchRatio = resonatorKeytrack ? voice.getValue(resonatorParams.pitch) : 1.0f;
        const bool loopFilterKeytrack = resonatorParams.loopFilterKeytrack->isOn();
        const bool postFilterKeytrack = resonatorParams.postFilterKeytrack->isOn();
        const float loopFilterMode = resonatorParams.loopFilterType->getProcValue() * 0.5f;

        for (int channel = 0; channel < WaveguideControlFrame::channelCount; ++channel)
        {
            const auto laneIndex = WaveguideKernel::laneIndex(resonator, channel);
            auto& lane = controlFrame.lanes[static_cast<std::size_t>(laneIndex)];
            lane.enabled = enabled;
            lane.gain = gain;
            if (!enabled)
                continue;

            const auto requestedFrequency = resonatorKeytrack
                                                ? resonatorPitchRatio * newFrequency
                                                : voice.getValue(resonatorParams.frequency, channel);
            lane.frequencyHz = std::max(minimumFrequency, requestedFrequency);
            lane.decaySeconds = voice.getValue(resonatorParams.decayTime, channel);
            lane.dispersion = voice.getValue(resonatorParams.dispersion, channel);

            auto& ratio = pitchRatioCache[static_cast<std::size_t>(laneIndex)];
            float loopFilterRatio = 1.0f;
            if (loopFilterKeytrack)
            {
                const auto semitones = voice.getValue(resonatorParams.loopFilterPitchInSemis, channel);
                if (!ratio.loopInitialised || semitones != ratio.loopSemitones)
                {
                    ratio.loopSemitones = semitones;
                    ratio.loopRatio = std::pow(2.0f, semitones * semitonesToOctaves);
                    ratio.loopInitialised = true;
                }
                loopFilterRatio = ratio.loopRatio;
            }
            lane.loopFilter.frequencyHz = loopFilterKeytrack
                                              ? lane.frequencyHz * loopFilterRatio
                                              : voice.getValue(resonatorParams.loopFilterCutoff, channel);
            lane.loopFilter.resonance = voice.getValue(resonatorParams.loopFilterResonance, channel) + 0.001f;
            lane.loopFilter.mode = loopFilterMode;
            lane.loopFilter.frequencyIsPeak = loopFilterKeytrack;

            float postFilterRatio = 1.0f;
            if (postFilterKeytrack)
            {
                const auto semitones = voice.getValue(resonatorParams.postFilterPitchInSemis, channel);
                if (!ratio.postInitialised || semitones != ratio.postSemitones)
                {
                    ratio.postSemitones = semitones;
                    ratio.postRatio = std::pow(2.0f, semitones * semitonesToOctaves);
                    ratio.postInitialised = true;
                }
                postFilterRatio = ratio.postRatio;
            }
            lane.postFilter.frequencyHz = postFilterKeytrack
                                              ? lane.frequencyHz * postFilterRatio
                                              : voice.getValue(resonatorParams.postFilterCutoff, channel);
            lane.postFilter.resonance = voice.getValue(resonatorParams.postFilterResonance, channel);
            lane.postFilter.mode = voice.getValue(resonatorParams.postFilterMode, channel);
            lane.postFilter.frequencyIsPeak = false;
        }
    }

    kernel.update(controlFrame, numSamples, WaveguideRetuneMode::crossfade);
}

void WaveguideResonatorBank::processKernel(juce::dsp::AudioBlock<float>& exciterBlock,
                                          juce::dsp::AudioBlock<float>& previousResonatorBankBlock,
                                          juce::dsp::AudioBlock<float>& outputBlock) noexcept
{
    using namespace resonarium::waveguide;

    jassert(voice.waveguideScratchArena.isPrepared());
    if (!voice.waveguideScratchArena.isPrepared())
    {
        outputBlock.clear();
        return;
    }

    auto bankInputBlock = voice.waveguideScratchArena.getBlock(
        WaveguideScratchArena::Region::bankInput, static_cast<int>(outputBlock.getNumSamples()));
    WaveguideProcessView view;
    view.numSamples = static_cast<int>(outputBlock.getNumSamples());
    for (int channel = 0; channel < WaveguideControlFrame::channelCount; ++channel)
    {
        const auto channelIndex = static_cast<std::size_t>(channel);
        view.exciter[channelIndex] = exciterBlock.getChannelPointer(channelIndex);
        view.previousBank[channelIndex] = previousResonatorBankBlock.getChannelPointer(channelIndex);
        view.bankInput[channelIndex] = bankInputBlock.getChannelPointer(channelIndex);
        view.output[channelIndex] = outputBlock.getChannelPointer(channelIndex);
        view.solo[channelIndex] = controlFrame.soloResonator >= 0
                                      ? voice.soloBuffer.getWritePointer(channel, voice.currentBlockStartSample)
                                      : nullptr;
    }
    kernel.process(view);
}
