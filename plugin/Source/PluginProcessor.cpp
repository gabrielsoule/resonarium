#include "PluginProcessor.h"
#include "PluginEditor.h"

gin::ProcessorOptions ResonariumProcessor::getOptions()
{
    gin::ProcessorOptions options;
    // options.wantsMidi = true;
    // options.useNewsChecker = false;
    // options.useUpdateChecker = false;
    return options;
}

//==============================================================================
ResonariumProcessor::ResonariumProcessor() : gin::Processor(
                                                 false), synth(*this)
{
    lf = std::make_unique<gin::CopperLookAndFeel>();
    exciterParams.setup(*this);

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

    setupModMatrix();
    init();
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
    synth.clearVoices();
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
}

// bool ResonariumProcessor::isBusesLayoutSupported (const BusesLayout& layout) const
// {
//     if (layout.inputBuses.size() != 0 || layout.outputBuses.size() != 1)
//         return false;
//
//     bool result =  layout.outputBuses[0].size() == 2;
//     DBG("Buses layout supported: " + juce::String(result ? "yes" : "no") + "!)");
//     return result;
// }

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

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    DBG("Instantiating new ResonariumProcessor instance...");
    return new ResonariumProcessor();
}

void ResonariumProcessor::ExciterParams::setup(ResonariumProcessor& p)
{
    attack = p.addExtParam("exciterAttack", "Attack", "A", "s",
                           {0.0, 30.0, 0.0, 0.2f}, 0.1f,
                           0.0f);

    decay = p.addExtParam("exciterDecay", "Decay", "D", "s",
                          {0.0, 30.0, 0.0, 0.2f}, 0.1f,
                          0.0f);

    sustain = p.addExtParam("exciterSustain", "Sustain", "S", "s",
                            {0.0, 100, 0.0, 1.0}, 50.0f,
                            0.0f);
    sustain->conversionFunction = [](const float x) { return x / 100.0f; };

    release = p.addExtParam("exciterRelease", "Release", "R", "s",
                            {0.0, 30.0, 0.0, 0.2f}, 0.1f,
                            0.0f);

    level = p.addExtParam("exciterLevel", "Level", "L", "dB",
                          {0.0, 60.0, 0.0, 0.2f}, 0.1f,
                          0.0f);
}
