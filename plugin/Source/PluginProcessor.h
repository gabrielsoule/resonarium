#pragma once

#include <JuceHeader.h>

#include "ResonatorSynth.h"
#include "Parameters.h"

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
    gin::ProcessorOptions getOptions();

    ResonatorSynth synth;
    gin::ModMatrix modMatrix;
    gin::ModSrcId
        modSrcPressure,
        modSrcTimbre,
        modSrcPitchbend,
        modSrcNote,
        modSrcVelocity;
    juce::Array<gin::ModSrcId> modSrcMonoLFO;
    juce::Array<gin::ModSrcId> modSrcMonoRND;
    juce::Array<gin::ModSrcId> modSrcPolyENV;
    //contains all the parameters that a voice needs modulated polyphonically (which is almost all of 'em)
    UIParams uiParams;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ResonariumProcessor)
};
