#include "PluginProcessor.h"

#include <array>
#include <cmath>
#include <cstdio>

namespace
{
constexpr int sampleRate = 48'000;
constexpr int preparedBlockSize = 1'024;
constexpr int automationSubBlockSize = 32;
constexpr int subBlocks = preparedBlockSize / automationSubBlockSize;

class LoadedWrapperScope
{
public:
    explicit LoadedWrapperScope(juce::AudioProcessor::WrapperType wrapper)
        : previous(juce::PluginHostType::getPluginLoadedAs())
    {
        juce::PluginHostType::jucePlugInClientCurrentWrapperType = wrapper;
        juce::AudioProcessor::setTypeOfNextNewPlugin(wrapper);
    }

    void processorConstructed()
    {
        juce::AudioProcessor::setTypeOfNextNewPlugin(
            juce::AudioProcessor::wrapperType_Undefined);
    }

    ~LoadedWrapperScope()
    {
        juce::AudioProcessor::setTypeOfNextNewPlugin(
            juce::AudioProcessor::wrapperType_Undefined);
        juce::PluginHostType::jucePlugInClientCurrentWrapperType = previous;
    }

private:
    juce::AudioProcessor::WrapperType previous;
};

bool hasOptionalStereoInstrumentInput(juce::AudioProcessor::WrapperType wrapper)
{
    juce::PluginHostType::jucePlugInClientCurrentWrapperType = wrapper;
    const auto properties = ResonariumProcessor::getBusesPropertiesForCurrentWrapper();
    return properties.inputLayouts.size() == 1
           && properties.inputLayouts[0].defaultLayout == juce::AudioChannelSet::stereo()
           && !properties.inputLayouts[0].isActivatedByDefault
           && properties.outputLayouts.size() == 1
           && properties.outputLayouts[0].defaultLayout == juce::AudioChannelSet::stereo()
           && properties.outputLayouts[0].isActivatedByDefault;
}
} // namespace

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;

    const auto previousWrapper = juce::PluginHostType::getPluginLoadedAs();
    const std::array nonLv2Wrappers {
        juce::AudioProcessor::wrapperType_VST3,
        juce::AudioProcessor::wrapperType_AudioUnit,
        juce::AudioProcessor::wrapperType_Standalone,
        juce::AudioProcessor::wrapperType_Undefined,
    };
    bool nonLv2OptionalInput = true;
    for (const auto wrapper : nonLv2Wrappers)
        nonLv2OptionalInput = nonLv2OptionalInput
                              && hasOptionalStereoInstrumentInput(wrapper);
    juce::PluginHostType::jucePlugInClientCurrentWrapperType = previousWrapper;

    LoadedWrapperScope wrapperScope(juce::AudioProcessor::wrapperType_LV2);
    ResonariumProcessor processor;
    wrapperScope.processorConstructed();
    processor.prepareToPlay(sampleRate, preparedBlockSize);

    auto& external = processor.synth.params.voiceParams.externalInputExciterParams;
    external.enabled->setUserValue(1.0f);
    external.mix->setUserValue(1.0f);

    juce::AudioBuffer<float> audio(2, automationSubBlockSize);
    juce::MidiBuffer midi;
    bool finite = true;
    float outputPeak = 0.0f;
    for (int block = 0; block < subBlocks; ++block)
    {
        for (int channel = 0; channel < audio.getNumChannels(); ++channel)
            for (int sample = 0; sample < audio.getNumSamples(); ++sample)
                audio.setSample(channel, sample,
                                0.1f * std::sin(static_cast<float>(block * automationSubBlockSize
                                                                   + sample + channel)));

        processor.processBlock(audio, midi);
        for (int channel = 0; channel < audio.getNumChannels(); ++channel)
            for (int sample = 0; sample < audio.getNumSamples(); ++sample)
            {
                const auto value = audio.getSample(channel, sample);
                finite = finite && std::isfinite(value);
                outputPeak = std::max(outputPeak, std::abs(value));
            }
    }

    const bool noInputBus = processor.getBusCount(true) == 0
                            && processor.getMainBusNumInputChannels() == 0
                            && processor.getBusCount(false) == 1
                            && processor.getMainBusNumOutputChannels() == 2;
    const bool constructedAsLv2 = processor.wrapperType
                                  == juce::AudioProcessor::wrapperType_LV2;
    const auto inputTelemetry = processor.synth.getExternalInputFrame().getTelemetry();
    const bool externalFrameStayedInvalid = !processor.synth.getExternalInputFrame().isValid()
                                             && inputTelemetry.validBlocks == 0
                                             && inputTelemetry.sourceSamplesCopied == 0;
    std::fprintf(stderr,
                 "instrument LV2 no-input gate: no_input_bus=%s non_lv2_optional_input=%s "
                 "constructed_as_lv2=%s finite=%s silent=%s "
                 "external_frame_invalid=%s peak=%.9g\n",
                 noInputBus ? "true" : "false",
                 nonLv2OptionalInput ? "true" : "false",
                 constructedAsLv2 ? "true" : "false",
                 finite ? "true" : "false",
                 outputPeak <= 1.0e-7f ? "true" : "false",
                 externalFrameStayedInvalid ? "true" : "false",
                 static_cast<double>(outputPeak));
    return noInputBus && nonLv2OptionalInput && constructedAsLv2 && finite
                   && outputPeak <= 1.0e-7f
                   && externalFrameStayedInvalid
               ? 0
               : 1;
}
