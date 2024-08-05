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

    uiParams.setup(*this);

    //Synth setup
    synth.enableLegacyMode();
    synth.setVoiceStealingEnabled(true);
    synth.setMPE(true);
    for (int i = 0; i < NUM_SYNTH_VOICES; i++)
    {
        ResonatorVoice* voice = new ResonatorVoice(*this, synth.params.voiceParams);
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

    for (int i = 0; i < NUM_LFOS; i++)
    {
        modSrcMonoLFO.add(modMatrix.addMonoModSource(juce::String::formatted("mlfo%d", i + 1),
                                                     juce::String::formatted("LFO %d (Mono)", i + 1),
                                                     true));

        modSrcPolyLFO.add(modMatrix.addPolyModSource(juce::String::formatted("lfo%d", i + 1),
                                                     juce::String::formatted("LFO %d", i + 1),
                                                     true));
    }

    for (int i = 0; i < NUM_RANDOMS; i++)
    {
        modSrcMonoRND.add(modMatrix.addMonoModSource(juce::String::formatted("mrnd%d", i + 1),
                                                     juce::String::formatted("RAND %d (Mono)", i + 1),
                                                     true));
        modSrcPolyRND.add(modMatrix.addPolyModSource(juce::String::formatted("mrnd%d", i + 1),
                                                     juce::String::formatted("RAND %d", i + 1),
                                                     true));
    }

    for (int i = 0; i < NUM_ENVELOPES; i++)
    {
        modSrcPolyENV.add(modMatrix.addPolyModSource(juce::String::formatted("env%d", i + 1),
                                                     juce::String::formatted("ENV %d", i + 1),
                                                     false));
    }

    for (int i = 0; i < NUM_MSEGS; i++)
    {
        modSrcMonoMSEG.add(modMatrix.addMonoModSource(juce::String::formatted("mmseg%d", i + 1),
                                                      juce::String::formatted("MSEG %d (Mono)", i + 1),
                                                      true));
        modSrcPolyMSEG.add(modMatrix.addPolyModSource(juce::String::formatted("env%d", i + 1),
                                                      juce::String::formatted("MSEG %d", i + 1),
                                                      false));
    }


    for (int i = 0; i <= 119; i++)
    {
        juce::String name = juce::MidiMessage::getControllerName (i);
        if (name.isEmpty())
            modSrcCC.add (modMatrix.addMonoModSource (juce::String::formatted ("cc%d", i), juce::String::formatted ("CC %d", i), false));
        else
            modSrcCC.add (modMatrix.addMonoModSource (juce::String::formatted ("cc%d", i), juce::String::formatted ("CC %d ", i) + name, false));
    }

    for (auto pp : getPluginParameters())
    {
        if (!pp->isInternal())
            DBG("  Adding parameter " + pp->getName(40) + " to mod matrix as a poly parameter");
        modMatrix.addParameter(pp, true, 0.02);
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
    modMatrix.setSampleRate(newSampleRate);
    synth.prepare({newSampleRate, static_cast<juce::uint32>(newSamplesPerBlock), 2});
    inputBuffer = juce::AudioBuffer<float>(2, newSamplesPerBlock);
    inputBuffer.clear();

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
    inputBuffer.copyFrom(0, 0, buffer.getReadPointer(0), buffer.getNumSamples());
    inputBuffer.copyFrom(1, 0, buffer.getReadPointer(1), buffer.getNumSamples());
    buffer.applyGain(std::cos(synth.params.voiceParams.externalInputExciterParams.mix->getProcValue() * juce::MathConstants<float>::halfPi));
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
    auto* editor = new gin::ScaledPluginEditor(new ResonariumEditor(*this), state);
    editor->editor->resized();
    return editor;
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    DBG("Instantiating new ResonariumProcessor instance!");
    return new ResonariumProcessor();
}
