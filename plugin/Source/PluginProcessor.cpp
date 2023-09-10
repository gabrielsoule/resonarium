#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
Processor::Processor() : gin::Processor (false)
{
    lf = std::make_unique<gin::CopperLookAndFeel>();

    init();
}

Processor::~Processor()
{
}

//==============================================================================
void Processor::stateUpdated()
{
}

void Processor::updateState()
{
}

void Processor::reset()
{
}

void Processor::prepareToPlay (double newSampleRate, int newSamplesPerBlock)
{
    juce::ignoreUnused (newSampleRate, newSamplesPerBlock);
}

void Processor::releaseResources()
{
}

void Processor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ignoreUnused (buffer, midi);
    juce::ScopedNoDenormals noDenormals;
}

//==============================================================================
bool Processor::hasEditor() const
{
    return true;
}

juce::AudioProcessorEditor* Processor::createEditor()
{
    return new gin::ScaledPluginEditor (new Editor (*this), state);
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new Processor();
}
