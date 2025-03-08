#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "ResonatorVoice.h"
#include "ui/ResonariumLookAndFeel.h"
#include "BinaryData.h"
#include "util/ResonariumUtilities.h"

gin::ProcessorOptions ResonariumProcessor::getOptions()
{
    gin::ProcessorOptions options;
    options.programmingCredits.clear();
    options.programmingCredits.add("Gabriel Soule");
    options.developer = "Gabriel Soule";
#if RESONARIUM_IS_INSTRUMENT
    options.pluginVersion = "0.0.8 (INST) ALPHA";
#endif
#if RESONARIUM_IS_EFFECT
    options.pluginVersion = "0.0.8 (FX) ALPHA";
#endif
    options.pluginName = "Resonarium";
    return options;
}

//==============================================================================
ResonariumProcessor::ResonariumProcessor() :
    gin::Processor(false, getOptions(),
                   juce::JSON::parse(juce::String(BinaryData::tooltips_json, BinaryData::tooltips_jsonSize))),
    synth(globalState, SynthParams(*this)),
    uiParams(*this)
{
#if PERFETTO
    DBG("Perfetto is ENABLED!");
    MelatoninPerfetto::get().beginSession();
#endif

    lf = std::make_unique<ResonariumLookAndFeel>();

    auto sz = 0;
    for (auto i = 0; i < BinaryData::namedResourceListSize; i++)
    {
        if (juce::String(BinaryData::originalFilenames[i]).endsWith(".xml"))
        {
            if (auto data = BinaryData::getNamedResource(BinaryData::namedResourceList[i], sz))
            {
                extractProgram(BinaryData::originalFilenames[i], data, sz);
                DBG("Loading built-in preset: " + juce::String (BinaryData::originalFilenames[i]));
            }
        }
    }

    synth.setMPE(true);
    synth.enableLegacyMode(48);
    synth.setVoiceStealingEnabled(true);
    for (int i = 0; i < 64; i++)
    {
        ResonatorVoice* voice = new ResonatorVoice(globalState, synth.params.voiceParams);
        globalState.modMatrix.addVoice(voice);
        synth.addVoice(voice);
        voice->id = i;
    }
    setupModMatrix(); //set up the modulation matrix
    synth.setNumVoices(NUM_SYNTH_VOICES);
    init(); //internal init
}

ResonariumProcessor::~ResonariumProcessor()
{
#if PERFETTO
    MelatoninPerfetto::get().endSession();
#endif
}

void ResonariumProcessor::setupModMatrix()
{
    globalState.modSrcFrequency = globalState.modMatrix.addPolyModSource("mfreq", "Note Frequency", false);
    globalState.modSrcPressure = globalState.modMatrix.addPolyModSource("mpep", "MPE Pressure", false);
    globalState.modSrcTimbre = globalState.modMatrix.addPolyModSource("mpet", "MPE Timbre", false);
    globalState.modSrcPitchbend = globalState.modMatrix.addMonoModSource("pb", "Pitch Bend", true);
    globalState.modSrcNote = globalState.modMatrix.addPolyModSource("note", "MIDI Note Number", false);
    globalState.modSrcVelocity = globalState.modMatrix.addPolyModSource("vel", "MIDI Velocity", false);

    for (int i = 0; i < NUM_LFOS; i++)
    {
        globalState.modSrcMonoLFO[i] = globalState.modMatrix.addMonoModSource(juce::String::formatted("mlfo%d", i + 1),
                                                                              juce::String::formatted(
                                                                                  "LFO %d (Mono)", i + 1),
                                                                              true);

        globalState.modSrcPolyLFO[i] = globalState.modMatrix.addPolyModSource(juce::String::formatted("lfo%d", i + 1),
                                                                              juce::String::formatted("LFO %d", i + 1),
                                                                              true);
    }

    for (int i = 0; i < NUM_RANDOMS; i++)
    {
        globalState.modSrcMonoRND[i] = globalState.modMatrix.addMonoModSource(juce::String::formatted("mrnd%d", i + 1),
                                                                              juce::String::formatted(
                                                                                  "RAND %d (Mono)", i + 1),
                                                                              true);

        globalState.modSrcPolyRND[i] = globalState.modMatrix.addPolyModSource(juce::String::formatted("rnd%d", i + 1),
                                                                              juce::String::formatted("RAND %d", i + 1),
                                                                              true);
    }

    for (int i = 0; i < NUM_ENVELOPES; i++)
    {
        globalState.modSrcPolyENV[i] = globalState.modMatrix.addPolyModSource(juce::String::formatted("env%d", i + 1),
                                                                              juce::String::formatted("ENV %d", i + 1),
                                                                              false);
    }

    for (int i = 0; i < NUM_MSEGS; i++)
    {
        globalState.modSrcMonoMSEG[i] = globalState.modMatrix.addMonoModSource(
            juce::String::formatted("mmseg%d", i + 1),
            juce::String::formatted("MSEG %d (Mono)", i + 1),
            true);


        globalState.modSrcPolyMSEG[i] = globalState.modMatrix.addPolyModSource(juce::String::formatted("mseg%d", i + 1),
                                                                               juce::String::formatted(
                                                                                   "MSEG %d", i + 1),
                                                                               false);
    }

    for (int i = 0; i < NUM_MACROS; i++)
    {
        globalState.modSrcMacro[i] = globalState.modMatrix.addMonoModSource(juce::String::formatted("macro%d", i + 1),
                                                                            juce::String::formatted("Macro %d", i + 1),
                                                                            false);
    }

    for (int i = 0; i <= 119; i++)
    {
        juce::String name = juce::MidiMessage::getControllerName(i);
        if (name.isEmpty())
            globalState.modSrcCC[i] = globalState.modMatrix.addMonoModSource(juce::String::formatted("cc%d", i),
                                                                             juce::String::formatted("CC %d", i),
                                                                             false);
        else
            globalState.modSrcCC[i] = globalState.modMatrix.addMonoModSource(juce::String::formatted("cc%d", i),
                                                                             juce::String::formatted("CC %d ", i) +
                                                                             name, false);
    }

    for (auto pp : getPluginParameters())
    {
        //TODO Add a way to choose whether parameters are mono or poly, right now everything is poly
        if (!pp->isInternal())
        {
            DBG("  Adding parameter " + pp->getName(40) + " with id " + pp->getParameterID() +
                " to mod matrix as a poly parameter");
            globalState.modMatrix.addParameter(pp, true, 0.02);
        }
    }
    DBG("TOTAL PARAMETERS REGISTERED: " + juce::String(getPluginParameters().size()));

    globalState.modMatrix.build();
}

//==============================================================================
void ResonariumProcessor::stateUpdated()
{
    DBG("Updating state FROM disk");
    globalState.modMatrix.stateUpdated(state);

    for (int i = 0; i < NUM_MSEGS; i++)
    {
        juce::String msegName = "MSEG" + juce::String(i + 1);
        auto msegTree = state.getChildWithName(msegName);

        if (msegTree.isValid())
        {
            try
            {
                synth.params.msegParams[i].msegData->reset();
                synth.params.msegParams[i].msegData->fromValueTree(msegTree);
            }
            catch (const std::exception& e)
            {
                DBG("Error loading MSEG " << (i+1) << ": " << e.what());
                synth.params.msegParams[i].msegData->reset();
            }
        }
        else
        {
            synth.params.msegParams[i].msegData->reset();
        }
    }

    //load the sample file path from the xml
    auto pathXML = state.getChildWithName("samplePath");

    //if the sample path is invalid or empty, clear the sampler
    if (!pathXML.isValid() || pathXML.getProperty("path").toString().isEmpty())
    {
        DBG("Processor: No sample path found in preset, clearing sampler");
        globalState.sampler.clear();
    }
    else
    {
        DBG("Processor: Loading sample from path: " + pathXML.getProperty("path").toString());
        globalState.sampler.loadFile(pathXML.getProperty("path").toString());
        globalState.samplePath = pathXML.getProperty("path").toString();
    }

    //force an editor update, this is a little janky and should be improved
    if (getActiveEditor())
    {
        auto* editor = getActiveEditor();
        jassert(editor != nullptr);
        auto* scaledEditor = dynamic_cast<gin::ScaledPluginEditor*>(editor);
        jassert(scaledEditor != nullptr);
        auto* resonariumEditor = dynamic_cast<ResonariumEditor*>(scaledEditor->editor.get());
        resonariumEditor->sampleExciterParamBox->sampleDropper->updateFromSampler();
    }

    globalState.logPrefix = "[" + getProgramName(getCurrentProgram()) + "] ";
    DBG(globalState.logPrefix + " State updated from disk successfully!");

    if (prepared) reset();
}

void ResonariumProcessor::updateState()
{
    DBG("Updating plugin state TO disk");
    globalState.modMatrix.updateState(state);
    for (int i = 0; i < NUM_MSEGS; i++)
    {
        auto msegTree = state.getOrCreateChildWithName("MSEG" + juce::String(i + 1), nullptr);
        synth.params.msegParams[i].msegData->toValueTree(msegTree);
    }

    //write the sample path from the sampler to the xml
    state.getOrCreateChildWithName("samplePath", nullptr).setProperty("path", globalState.sampler.getFilePath(),
                                                                      nullptr);
}

void ResonariumProcessor::reset()
{
    Processor::reset();
    synth.turnOffAllVoices(false);
}

void ResonariumProcessor::prepareToPlay(double newSampleRate, int newSamplesPerBlock)
{
    prepared = true;
    Processor::prepareToPlay(newSampleRate, newSamplesPerBlock);
    globalState.modMatrix.setSampleRate(newSampleRate);
    synth.prepare({newSampleRate, static_cast<juce::uint32>(newSamplesPerBlock), 2});
    globalState.extInputBuffer = juce::AudioBuffer<float>(2, newSamplesPerBlock);
    globalState.extInputBuffer.clear();
    reset();
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

    //toggle the constant note on or off based on the note parameters
    bool constantNote = synth.params.voiceParams.externalInputExciterParams.constantNote->isOn();
    if (constantNote && !constantNoteActive)
    {
        DBG(globalState.logPrefix + "Constant note toggled ON, sending note on message to synth");
        midi.addEvent(juce::MidiMessage::noteOn(constantNoteChannel, constantNoteNumber, constantNoteVelocity), 0);
        constantNoteActive = true;
        const float freq = synth.params.voiceParams.externalInputExciterParams.constantNoteFrequency->getProcValue();
        int pitchBend = ResonariumUtilities::calculateMPEPitchBendForFrequency(freq, constantNoteNumber, 48.0f);
        midi.addEvent(juce::MidiMessage::pitchWheel(constantNoteChannel, pitchBend), 0);
        constantNoteFrequency = freq;
    }
    else if (!constantNote && constantNoteActive)
    {
        DBG(globalState.logPrefix + "Constant note toggled OFF, sending note off message to synth");
        midi.addEvent(juce::MidiMessage::noteOff(constantNoteChannel, constantNoteNumber), 0);
        constantNoteActive = false;
    }

    if (constantNoteActive)
    {
        //update the constant note MPE pitchbend to the constant note frequency parameter
        const float freq = synth.params.voiceParams.externalInputExciterParams.constantNoteFrequency->getProcValue();
        if (freq != constantNoteFrequency)
        {
            DBG(globalState.logPrefix + "Constant note frequency changed, updating pitch bend to " + juce::String(freq));
            int pitchBend = ResonariumUtilities::calculateMPEPitchBendForFrequency(freq, constantNoteNumber, 48.0f);
            midi.addEvent(juce::MidiMessage::pitchWheel(constantNoteChannel, pitchBend), 0);
            constantNoteFrequency = freq;
        }

        //if we see a all notes off or all sound off message, retrigger the constant note on the next block
        for (const auto metadata : midi)
        {
            const auto msg = metadata.getMessage();
            if (msg.isAllNotesOff() || msg.isAllSoundOff())
            {
                constantNoteActive = false;
                DBG(globalState.logPrefix + "All notes off message processed. Toggling constant note OFF for one block; it will be retriggered next block.");
            }
        }
    }


    //deal with external input for the External Input Exciter
    bool extInputEnabled = synth.params.voiceParams.externalInputExciterParams.enabled->isOn();
    if (extInputEnabled)
    {
        globalState.extInputBuffer.copyFrom(0, 0, buffer.getReadPointer(0), buffer.getNumSamples());
        globalState.extInputBuffer.copyFrom(1, 0, buffer.getReadPointer(1), buffer.getNumSamples());
        globalState.extInputBuffer.applyGain(0.05f); //for some reason, external input is super loud for most signals and blows up the resonators
        buffer.applyGain(std::cos(
            synth.params.voiceParams.externalInputExciterParams.mix->getProcValue() *
            juce::MathConstants<float>::halfPi));
    }

#if RESONARIUM_IS_INSTRUMENT
    if (!buffer.hasBeenCleared()) buffer.clear();
#endif

    //update the global state for the rest of the program
    globalState.playHead = getPlayHead();

    //process audio
    synth.startBlock();
    synth.renderNextBlock(buffer, midi, 0, buffer.getNumSamples());
    globalState.modMatrix.finishBlock(buffer.getNumSamples());
    synth.endBlock(buffer.getNumSamples());
    midi.clear();
    globalState.extInputBuffer.clear();

#if JUCE_DEBUG
    //check the buffer for NaNs
    for (int i = 0; i < buffer.getNumChannels(); i++)
    {
        for (int j = 0; j < buffer.getNumSamples(); j++)
        {
            if (std::isnan(buffer.getSample(i, j)))
            {
                DBG(globalState.logPrefix + " NaN detected in channel " + juce::String(i) + " at sample " + juce::String(j));
                juce::String blockString = "";
                for (int k = 0; k < buffer.getNumSamples(); k++)
                {
                    blockString += juce::String(buffer.getSample(i, k)) + " ";
                }
                DBG(blockString);
                if (synth.getNumActiveVoices() == 0)
                {
                    DBG(globalState.logPrefix + "However, no active voices are present.");
                }
                else
                {
                    jassertfalse;
                }
                break;
            }
        }
    }

    int numMidiEvents = midi.getNumEvents();
    if (numMidiEvents > 0)
    {
        for (auto it = midi.findNextSamplePosition(0); it != midi.cend(); ++it)
        {
            const auto metadata = *it;
            const auto msg = metadata.getMessage();
            if (msg.isNoteOn())
            {
                DBG("[" + getProgramName(getCurrentProgram()) + "] Note On: " + juce::String(msg.getNoteNumber()) +
                    " at " + juce::String(metadata.samplePosition));
            }
        }
    }
#endif

    if (buffer.getNumSamples() <= scopeFifo.getFreeSpace() && buffer.getNumChannels() == scopeFifo.getNumChannels())
        scopeFifo.write(buffer);
}

//==============================================================================
bool ResonariumProcessor::hasEditor() const
{
    return true;
}

juce::AudioProcessorEditor* ResonariumProcessor::createEditor()
{
    DBG("Instantiating new ResonariumEditor instance!");
    auto* editor = new gin::ScaledPluginEditor(new ResonariumEditor(*this), state);
    editor->editor->resized();
    return editor;
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    DBG("Instantiating new ResonariumProcessor instance!");
    return new ResonariumProcessor();
}
