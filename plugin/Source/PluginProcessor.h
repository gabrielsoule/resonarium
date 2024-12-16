#pragma once

#include <JuceHeader.h>

#include "ResonatorSynth.h"
#include "Parameters.h"
#include <melatonin_perfetto/melatonin_perfetto.h>

#include "dsp/Sampler.h"

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
    static static gin::ProcessorOptions getOptions();

    ResonatorSynth synth;
    gin::ModMatrix modMatrix;
    gin::ModSrcId
        modSrcPressure,
        modSrcTimbre,
        modSrcPitchbend,
        modSrcNote,
        modSrcVelocity;
    juce::Array<gin::ModSrcId>
        modSrcMonoLFO,
        modSrcPolyLFO,
        modSrcMonoRND,
        modSrcPolyRND,
        modSrcPolyENV,
        modSrcMonoMSEG,
        modSrcPolyMSEG,
        modSrcCC,
        modSrcMacro;
    //contains all the parameters that a voice needs modulated polyphonically (which is almost all of 'em)
    UIParams uiParams;
    gin::AudioFifo scopeFifo { 2, 44100 };

    //A copy of the input buffer, which is accessed by Ext. In exciters.
    juce::AudioBuffer<float> inputBuffer;
    //every sample exciter is polyphonic, they all share the same sample buffer.
    Sampler sampler;
    juce::String samplePath = "";
    int pluginInstanceID = -1;
    bool prepared = false; //indicates that prepareToPlay has been called at least once
    juce::String logPrefix = "[default]"; //a prefix that identifies this instance, for debugging

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
