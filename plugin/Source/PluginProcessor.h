#pragma once

#include "ResonatorSynth.h"
#include "Parameters.h"
#include <melatonin_perfetto/melatonin_perfetto.h>
#include "GlobalState.h"
#include <gin/gin.h>
#include <gin_plugin/gin_plugin.h>

//==============================================================================
class ResonariumProcessor : public gin::Processor
{
public:
    ResonariumProcessor();
    ~ResonariumProcessor() override;

    void stateUpdated() override;
    void updateState() override;
    void reset() override;
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;
    bool supportsMPE() const override { return true; }
    void setupModMatrix();
    static gin::ProcessorOptions getOptions();
    
    // Get the synthesizer instance (needed for Python bindings)
    ResonatorSynth* getSynth() { return &synth; }

    ResonatorSynth synth;
    UIParams uiParams;
    GlobalState globalState;
    gin::AudioFifo scopeFifo { 2, 44100 };
    int pluginInstanceID = -1;
    bool prepared = false; //indicates that prepareToPlay has been called at least once
    bool soloActive = false; //true if a resonator is in solo mode

    bool simulateConstantNote;
    bool constantNoteActive;
    const int constantNoteNumber = 48 + 5;
    const float constantNoteVelocity = 0.5f;
    const int constantNoteChannel = 15;
    float constantNoteFrequency = 440.0f;

#if PERFETTO
    std::unique_ptr<perfetto::TracingSession> tracingSession;
#endif

    static bool checkBufferForNaN(juce::dsp::AudioBlock<float> block, juce::String caller = "")
    {
        for (size_t channelIdx = 0; channelIdx < block.getNumChannels(); channelIdx++)
        {
            for (size_t sampleIdx = 0; sampleIdx < block.getNumSamples(); sampleIdx++)
            {
                if (std::isnan(block.getSample(channelIdx, sampleIdx)))
                {
                    DBG(caller + ": NaN detected in channel " + juce::String(channelIdx) + " at sample " + juce::String(sampleIdx));
                    juce::Logger::writeToLog(caller + ": NaN detected in channel " + juce::String(channelIdx) + " at sample " + juce::String(sampleIdx));
                    juce::String blockString = "";
                    for (size_t k = 0; k < block.getNumSamples(); k++)
                    {
                        blockString += juce::String(block.getSample(channelIdx, k)) + " ";
                    }
                    DBG(blockString);
                    juce::Logger::writeToLog(blockString);
                    return true;
                }
            }
        }

        return false;
    }
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ResonariumProcessor)
};
