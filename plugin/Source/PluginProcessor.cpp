#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "ResonatorVoice.h"
#include "ui/ResonariumLookAndFeel.h"

gin::ProcessorOptions ResonariumProcessor::getOptions()
{
    gin::ProcessorOptions options;
    //no-op for now...
    return options;
}

//==============================================================================
ResonariumProcessor::ResonariumProcessor() : gin::Processor(
                                                 false), synth(*this)
{
    lf = std::make_unique<ResonariumLookAndFeel>();

    voiceParams.setup(*this);

    //Synth setup
    synth.enableLegacyMode();
    synth.setVoiceStealingEnabled(true);
    synth.setMPE(true);
    for (int i = 0; i < 16; i++)
    {
        auto voice = new ResonatorVoice(*this, voiceParams);
        modMatrix.addVoice(voice);
        synth.addVoice(voice);
        voice->id = i;
    }

    setupModMatrix(); //set up the modulation matrix
    init(); //internal init

}

ResonariumProcessor::~ResonariumProcessor()
{
}

void ResonariumProcessor::setupModMatrix()
{
    modSrcPressure = modMatrix.addPolyModSource("mpep", "MPE Pressure", false);
    modSrcTimbre = modMatrix.addPolyModSource("mpet", "MPE Timbre", false);
    modSrcPitchbend = modMatrix.addMonoModSource("pb", "Pitch Bend", true);
    modSrcNote = modMatrix.addPolyModSource("note", "MIDI Note Number", false);
    modSrcVelocity = modMatrix.addPolyModSource("vel", "MIDI Velocity", false);

    for (auto pp : getPluginParameters())
    {
        if (!pp->isInternal())
            DBG("Adding parameter " + pp->getName(40) + " to mod matrix as a poly parameter");
        modMatrix.addParameter(pp, true);
    }

    DBG("TOTAL PARAMETERS REGISTERED: " + juce::String(getPluginParameters().size()));

    modMatrix.build();
}

//==============================================================================
void ResonariumProcessor::stateUpdated()
{
    modMatrix.stateUpdated(state);
}

void ResonariumProcessor::updateState()
{
    modMatrix.updateState(state);
}

void ResonariumProcessor::reset()
{
    Processor::reset();
    synth.turnOffAllVoices(false);
}

void ResonariumProcessor::prepareToPlay(double newSampleRate, int newSamplesPerBlock)
{
    Processor::prepareToPlay(newSampleRate, newSamplesPerBlock);
    synth.prepare({newSampleRate, static_cast<juce::uint32>(newSamplesPerBlock), 2});
    modMatrix.setSampleRate(newSampleRate);

    DBG("Resonarium instance preparing to play:");
    DBG("   Input channels: " + juce::String(getMainBusNumInputChannels()));
    DBG("   Output channels: " + juce::String(getMainBusNumOutputChannels()));
    DBG("   Sample rate: " + juce::String(newSampleRate));
    DBG("   Samples per block: " + juce::String(newSamplesPerBlock));
}

void ResonariumProcessor::releaseResources()
{
    DBG("Releasing resources...");
}

void ResonariumProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    synth.startBlock();
    synth.renderNextBlock(buffer, midi, 0, buffer.getNumSamples());
    modMatrix.finishBlock(buffer.getNumSamples());
    synth.endBlock(buffer.getNumSamples());
}

//==============================================================================
bool ResonariumProcessor::hasEditor() const
{
    return true;
}

juce::AudioProcessorEditor* ResonariumProcessor::createEditor()
{
    return new gin::ScaledPluginEditor(new ResonariumEditor(*this, voiceParams), state);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    DBG("Instantiating new ResonariumProcessor instance!");
    return new ResonariumProcessor();
}
