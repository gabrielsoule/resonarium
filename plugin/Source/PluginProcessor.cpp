#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "ResonatorVoice.h"

gin::ProcessorOptions ResonariumProcessor::getOptions()
{
    gin::ProcessorOptions options;
    //no-op for now...
    return options;
}

void ResonariumProcessor::distributeParameters()
{
    synth.distributeParameters();
    //maybe do more once we have more DSP components doing stuff?
}

//==============================================================================
ResonariumProcessor::ResonariumProcessor() : gin::Processor(
                                                 false), synth(*this)
{
    lf = std::make_unique<gin::CopperLookAndFeel>();

    // Parameter setup
    exciterParams.setup(*this);
    for (int i = 0; i < NUM_RESONATOR_BANKS; i++)
    {
        resonatorBanksParams[i] = ResonatorBankParams(i);
        resonatorBanksParams[i].setup(*this); // resonator bank handles individual resonator setup
    }

    //Synth setup
    synth.enableLegacyMode();
    synth.setVoiceStealingEnabled(true);
    synth.setMPE(true);
    for (int i = 0; i < 16; i++)
    {
        auto voice = new ResonatorVoice(*this);
        modMatrix.addVoice(voice);
        synth.addVoice(voice);
        voice->id = i;
    }

    //Mod Matrix setup
    setupModMatrix();

    //Internal init, required by gin/JUCE, loads all presets into memory
    init();

    //Finally, distribute parameter structs to their appropriate DSP components
    distributeParameters();
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
    return new gin::ScaledPluginEditor(new ResonariumEditor(*this), state);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    DBG("Instantiating new ResonariumProcessor instance!");
    return new ResonariumProcessor();
}
