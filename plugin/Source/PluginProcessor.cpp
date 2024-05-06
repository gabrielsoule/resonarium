#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
ResonariumProcessor::ResonariumProcessor() : gin::Processor(false), synth(*this)
{
    lf = std::make_unique<gin::CopperLookAndFeel>();
    exciterParams.setup(*this);
    init();
}

ResonariumProcessor::~ResonariumProcessor()
{
}

//==============================================================================
void ResonariumProcessor::stateUpdated()
{
}

void ResonariumProcessor::updateState()
{
}

void ResonariumProcessor::reset()
{
}

void ResonariumProcessor::prepareToPlay (double newSampleRate, int newSamplesPerBlock)
{
    juce::ignoreUnused (newSampleRate, newSamplesPerBlock);
    for(auto x : this->getParameters())
    {
        DBG(x->getName(40));
        DBG(x->getCurrentValueAsText());
    }
}

void ResonariumProcessor::releaseResources()
{
}

void ResonariumProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ignoreUnused (buffer, midi);
    juce::ScopedNoDenormals noDenormals;
}

//==============================================================================
bool ResonariumProcessor::hasEditor() const
{
    return true;
}

juce::AudioProcessorEditor* ResonariumProcessor::createEditor()
{
    return new gin::ScaledPluginEditor (new ResonariumEditor (*this), state);
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ResonariumProcessor();
}

void ResonariumProcessor::ExciterParams::setup(ResonariumProcessor& p)
{
    attack = p.addExtParam("exciterAttack", "Attack", "A", "s",
        {0.0, 60.0, 0.0, 0.2f}, 0.1f,
        gin::SmoothingType::Type::linear);

    decay = p.addExtParam("exciterDecay", "Decay", "D", "s",
        {0.0, 60.0, 0.0, 0.2f}, 0.1f,
        gin::SmoothingType::Type::linear);

    sustain = p.addExtParam("exciterSustain", "Sustain", "S", "s",
        {0.0, 100.0, 0.0, 1.0}, 0.1f,
        gin::SmoothingType::Type::linear);

    release = p.addExtParam("exciterRelease", "Release", "R", "s",
        {0.0, 60.0, 0.0, 0.2f}, 0.1f,
        gin::SmoothingType::Type::linear);

    level = p.addExtParam("exciterLevel", "Level", "L", "dB",
        {0.0, 60.0, 0.0, 0.2f}, 0.1f,
        gin::SmoothingType::Type::linear);
}

