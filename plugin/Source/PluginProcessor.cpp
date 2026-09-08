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
    options.lookAndFeel = std::make_unique<ResonariumLookAndFeel>();
#if RESONARIUM_IS_INSTRUMENT
    options.pluginVersion = "0.1.0 (INST) BETA";
#endif
#if RESONARIUM_IS_EFFECT
    options.pluginVersion = "0.1.0 (FX) BETA";
#endif
    options.pluginName = "Resonarium";
    return options;
}

juce::AudioProcessor::BusesProperties
ResonariumProcessor::getBusesPropertiesForCurrentWrapper()
{
    const auto stereo = juce::AudioChannelSet::stereo();
    auto properties = juce::AudioProcessor::BusesProperties().withOutput("Output", stereo, true);

#if RESONARIUM_IS_INSTRUMENT
    // JUCE 8.0.8's LV2 wrapper dereferences every advertised audio-input port,
    // including connection-optional ports that the host is allowed to leave
    // disconnected. Omitting the Instrument input bus in this one format keeps
    // JUCE pristine and makes the supported no-input synth topology explicit.
    if (juce::PluginHostType::getPluginLoadedAs()
        != juce::AudioProcessor::wrapperType_LV2)
        properties = properties.withInput("Input", stereo, false);
#else
    properties = properties.withInput("Input", stereo, true);
#endif

    return properties;
}

ResonariumProcessor::ResonariumProcessor() :
    gin::Processor(getBusesPropertiesForCurrentWrapper(), false, getOptions(),
                   juce::JSON::parse(juce::String(BinaryData::tooltips_json, BinaryData::tooltips_jsonSize))),
    synth(globalState, SynthParams(*this)),
    uiParams(*this)
{
#if PERFETTO
    DBG("Perfetto is ENABLED!");
    MelatoninPerfetto::get().beginSession();
#endif

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
    const bool voicePoolInitialised = synth.initialiseVoicePool([this](int index) -> gin::SynthesiserVoice*
    {
        ResonatorVoice* voice = new ResonatorVoice(globalState, synth.params.voiceParams,
                                                   synth.getWaveguideScratchArena());
        globalState.modMatrix.addVoice(voice);
        voice->id = index;
        return voice;
    });
    jassert(voicePoolInitialised);
    juce::ignoreUnused(voicePoolInitialised);
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
                synth.params.msegParams[i].msegData->replaceFromValueTree(msegTree);
            }
            catch (const std::exception& e)
            {
                DBG("Error loading MSEG " << (i+1) << ": " << e.what());
                synth.params.msegParams[i].msegData->resetToDefault();
            }
        }
        else
        {
            synth.params.msegParams[i].msegData->resetToDefault();
        }
    }

    //load the sample file path from the xml
    auto pathXML = state.getChildWithName("samplePath");

    //if the sample path is invalid or empty, clear the sampler
    if (!pathXML.isValid() || pathXML.getProperty("path").toString().isEmpty())
    {
        globalState.sampler.requestClear();
    }
    else
    {
        const auto samplePath = pathXML.getProperty("path").toString();
        static_cast<void>(globalState.sampler.requestLoadFile(juce::File(samplePath)));
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
        synth.params.msegParams[i].msegData->writeToValueTree(msegTree);
    }

    //write the sample path from the sampler to the xml
    state.getOrCreateChildWithName("samplePath", nullptr)
        .setProperty("path", globalState.sampler.getStatePath(), nullptr);
}

void ResonariumProcessor::reset()
{
    Processor::reset();
    synth.requestReset();
}

void ResonariumProcessor::prepareToPlay(double newSampleRate, int newSamplesPerBlock)
{
    prepared = true;
    Processor::prepareToPlay(newSampleRate, newSamplesPerBlock);
    globalState.modMatrix.setSampleRate(newSampleRate);
    synth.prepare({newSampleRate, static_cast<juce::uint32>(newSamplesPerBlock), 2});
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

bool ResonariumProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    // The complete exciter, resonator, modulation, and effect engine is stereo.
    // Reject layouts that cannot satisfy those invariants instead of advertising
    // JUCE's default "any layout" support and then dereferencing channel 1.
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    const auto input = layouts.getMainInputChannelSet();
#if RESONARIUM_IS_INSTRUMENT
    return input.isDisabled() || input == juce::AudioChannelSet::stereo();
#else
    return input == juce::AudioChannelSet::stereo();
#endif
}

void ResonariumProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;

    globalState.modMatrix.beginAudioBlock();
    globalState.bypassResonators = uiParams.bypassResonators->isOn();
    const bool panicThisBlock = synth.beginHostBlock(buffer.getNumSamples());
    if (panicThisBlock)
    {
        constantNoteActive = false;
        constantNoteSuppressed = true;
    }

    //toggle the constant note on or off based on the note parameters
    bool constantNote = synth.params.voiceParams.externalInputExciterParams.constantNote->isOn();
    if (!constantNote)
        constantNoteSuppressed = false;

    if (constantNoteActive)
    {
        const juce::MPENote constantNoteIdentity(
            constantNoteChannel, constantNoteNumber, juce::MPEValue::fromUnsignedFloat(constantNoteVelocity),
            juce::MPEValue::centreValue(), juce::MPEValue::minValue(), juce::MPEValue::minValue());
        constantNoteActive = synth.isNotePlaying(constantNoteIdentity);
    }

    if (constantNote && !constantNoteActive && !constantNoteSuppressed)
    {
        synth.processDirectMidiEvent(
            juce::MidiMessage::noteOn(constantNoteChannel, constantNoteNumber, constantNoteVelocity));
        constantNoteActive = true;
        const float freq = synth.params.voiceParams.externalInputExciterParams.constantNoteFrequency->getProcValue();
        int pitchBend = ResonariumUtilities::calculateMPEPitchBendForFrequency(freq, constantNoteNumber, 48.0f);
        synth.processDirectMidiEvent(juce::MidiMessage::pitchWheel(constantNoteChannel, pitchBend));
        constantNoteFrequency = freq;
    }
    else if (!constantNote && constantNoteActive)
    {
        synth.processDirectMidiEvent(juce::MidiMessage::noteOff(constantNoteChannel, constantNoteNumber));
        constantNoteActive = false;
    }

    if (constantNoteActive)
    {
        //update the constant note MPE pitchbend to the constant note frequency parameter
        const float freq = synth.params.voiceParams.externalInputExciterParams.constantNoteFrequency->getProcValue();
        if (freq != constantNoteFrequency)
        {
            int pitchBend = ResonariumUtilities::calculateMPEPitchBendForFrequency(freq, constantNoteNumber, 48.0f);
            synth.processDirectMidiEvent(juce::MidiMessage::pitchWheel(constantNoteChannel, pitchBend));
            constantNoteFrequency = freq;
        }

        //if we see a all notes off or all sound off message, retrigger the constant note on the next block
        for (const auto metadata : midi)
        {
            const auto msg = metadata.getMessage();
            if (msg.isAllSoundOff())
            {
                constantNoteActive = false;
                constantNoteSuppressed = true;
            }
            else if (msg.isAllNotesOff())
            {
                constantNoteActive = false;
            }
        }
    }


    //deal with external input for the External Input Exciter
    const bool extInputRequested = synth.params.voiceParams.externalInputExciterParams.enabled->isOn();
    const float extInputRawMix = synth.params.voiceParams.externalInputExciterParams.mix->getProcValue();
    const bool hasStereoInput = getMainBusNumInputChannels() >= 2 && buffer.getNumChannels() >= 2;
    const float* inputLeft = hasStereoInput ? buffer.getReadPointer(0) : nullptr;
    const float* inputRight = hasStereoInput ? buffer.getReadPointer(1) : nullptr;
    const float externalDryGain = synth.beginExternalInputBlock(inputLeft, inputRight,
                                                                 buffer.getNumSamples(),
                                                                 extInputRequested,
                                                                 extInputRawMix);
    if (synth.getExternalInputFrame().isValid())
        buffer.applyGain(externalDryGain);

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

#if JUCE_DEBUG
    // Keep debug validation allocation-free: no string construction or audio
    // dumps are permitted from the callback.
    for (int i = 0; i < buffer.getNumChannels(); i++)
    {
        for (int j = 0; j < buffer.getNumSamples(); j++)
        {
            if (!std::isfinite(buffer.getSample(i, j)))
            {
                buffer.clear();
                jassertfalse;
                break;
            }
        }
    }
#endif

    if (scopeConsumerCount.load(std::memory_order_acquire) != 0
        && buffer.getNumSamples() <= scopeFifo.getFreeSpace()
        && buffer.getNumChannels() == scopeFifo.getNumChannels())
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
