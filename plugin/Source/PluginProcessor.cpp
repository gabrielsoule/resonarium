#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "ResonatorVoice.h"
#include "ui/ResonariumLookAndFeel.h"

gin::ProcessorOptions ResonariumProcessor::getOptions()
{
    gin::ProcessorOptions options;
    options.programmingCredits.clear();
    options.programmingCredits.add("Gabriel Soule");
    options.developer = "Gabriel Soule";
    options.pluginVersion = "0.0.5 ALPHA";
    options.pluginName = "Resonarium";
    return options;
}

//==============================================================================
ResonariumProcessor::ResonariumProcessor() : gin::Processor(
                                                 false, getOptions()), synth(*this)
{
#if PERFETTO
    DBG("Perfetto is ENABLED!");
    MelatoninPerfetto::get().beginSession();
#endif

    lf = std::make_unique<ResonariumLookAndFeel>();
    //Load tooltips from binary data
    int size = BinaryData::tooltips_jsonSize;
    const char* jsonData = BinaryData::tooltips_json;

    if (jsonData != nullptr)
    {
        juce::String jsonContent(jsonData, size);
        getTooltipManager().loadFromJson(juce::JSON::parse(jsonContent));
    }
    else
    {
        juce::Logger::writeToLog("WARNING: Failed to load tooltips.json from BinaryData");
        jassertfalse;
    }

    //Load built-in presets from binary data
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

    uiParams.setup(*this);

    //Setup the synth class
    synth.setupParameters();
    synth.enableLegacyMode();
    synth.setVoiceStealingEnabled(true);
    synth.setMPE(true);
    for (int i = 0; i < 64; i++)
    {
        ResonatorVoice* voice = new ResonatorVoice(*this, synth.params.voiceParams);
        modMatrix.addVoice(voice);
        synth.addVoice(voice);
        voice->id = i;
    }
    synth.setNumVoices(NUM_SYNTH_VOICES);
    setupModMatrix(); //set up the modulation matrix
    init(); //internal init
    // testFilter(500, 0.3, 1/std::sqrt<float>(2.0), false);
    // testFilter(400, 0.0, 1 / std::sqrt(2), false);
    // testFilter(400, 0.01, 4, false);
    // testFilter(4000, 0.01, 6, false);
    // testFilter(900, 0.01, 15, false);
    // testFilter<false>(3000, 0.03, 14 + 1 / std::sqrt(2), false);
    //
    // testFilter<false>(3000, 0.02, 14 + 1 / std::sqrt(2), false);
    // testFilter<false>(3000, 0, 14 + 1 / std::sqrt(2), false);
    // testFilter(500, 0, 1, false, true);
    // testFilter(400, 0, 1 / std::sqrt(2), true, true);
    // testFilter(0.5, 0.5, false);
    // testFilter(0.5, 1, false);
    // testFilter(0.5, 2, false);
    // testFilter(0.5, 3, false);
    // testFilter(0.48, 3, false);
    // for(float m = 0; m <= 1; m += 0.2)
    // {
    //     for (float q = 0.1; q <= 10; q *= 2)
    //     {
    //         testFilter(m, q, false);
    //     }
    // }
}

ResonariumProcessor::~ResonariumProcessor()
{
#if PERFETTO
    MelatoninPerfetto::get().endSession();
#endif
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

    for (int i = 0; i < NUM_MACROS; i++)
    {
        modSrcMacro.add(modMatrix.addMonoModSource(juce::String::formatted("macro%d", i + 1),
                                                   juce::String::formatted("Macro %d", i + 1),
                                                   false));
    }

    for (int i = 0; i <= 119; i++)
    {
        juce::String name = juce::MidiMessage::getControllerName(i);
        if (name.isEmpty())
            modSrcCC.add(modMatrix.addMonoModSource(juce::String::formatted("cc%d", i),
                                                    juce::String::formatted("CC %d", i), false));
        else
            modSrcCC.add(modMatrix.addMonoModSource(juce::String::formatted("cc%d", i),
                                                    juce::String::formatted("CC %d ", i) + name, false));
    }

    for (auto pp : getPluginParameters())
    {
        //TODO Add a way to choose whether parameters are mono or poly, right now everything is poly
        if (!pp->isInternal())
        {
            DBG("  Adding parameter " + pp->getName(40) + " to mod matrix as a poly parameter");
            modMatrix.addParameter(pp, true, 0.02);
        }
    }
    DBG("TOTAL PARAMETERS REGISTERED: " + juce::String(getPluginParameters().size()));

    modMatrix.build();
}

//==============================================================================
void ResonariumProcessor::stateUpdated()
{
    DBG("Updating state FROM disk");
    modMatrix.stateUpdated(state);

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
        sampler.clear();
    }
    else
    {
        DBG("Processor: Loading sample from path: " + pathXML.getProperty("path").toString());
        sampler.loadFile(pathXML.getProperty("path").toString());
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

    reset();
}

void ResonariumProcessor::updateState()
{
    DBG("Updating plugin state TO disk");
    modMatrix.updateState(state);
    for (int i = 0; i < NUM_MSEGS; i++)
    {
        auto msegTree = state.getOrCreateChildWithName("MSEG" + juce::String(i + 1), nullptr);
        synth.params.msegParams[i].msegData->toValueTree(msegTree);
    }

    //write the sample path from the sampler to the xml
    state.getOrCreateChildWithName("samplePath", nullptr).setProperty("path", sampler.getFilePath(), nullptr);
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
    inputBuffer.copyFrom(0, 0, buffer.getReadPointer(0), buffer.getNumSamples());
    inputBuffer.copyFrom(1, 0, buffer.getReadPointer(1), buffer.getNumSamples());
    buffer.applyGain(std::cos(
        synth.params.voiceParams.externalInputExciterParams.mix->getProcValue() * juce::MathConstants<float>::halfPi));
    synth.startBlock();
    synth.renderNextBlock(buffer, midi, 0, buffer.getNumSamples());
    modMatrix.finishBlock(buffer.getNumSamples());
    synth.endBlock(buffer.getNumSamples());

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
