#pragma once

#include <JuceHeader.h>

#include "ResonatorSynth.h"
#include "defines.h"
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
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    // bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;
    bool supportsMPE() const override {return true;}
    void setupModMatrix();
    gin::ProcessorOptions getOptions();

    ResonatorSynth synth;
    gin::ModMatrix modMatrix;
    gin::ModSrcId modSrcPressure, modSrcTimbre, modSrcPitchbend, modSrcNote, modSrcVelocity;

    ExciterParams exciterParams;
    ResonatorBankParams resonatorBanksParams[NUM_RESONATOR_BANKS];

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ResonariumProcessor)
};
