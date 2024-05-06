#pragma once

#include <JuceHeader.h>

#include "ResonatorSynth.h"
#include "ResonatorVoice.h"

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

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;
    bool supportsMPE() const override {return true;}


    struct ExciterParams
    {
        gin::Parameter::Ptr attack, decay, sustain, release, level;

        void setup(ResonariumProcessor& p);
    };


    ResonatorSynth synth;
    gin::ModMatrix modMatrix;
    gin::ModSrcId modSrcPressure, modSrcTimbre, modSrcPitchbend, modSrcLFO, modSrcNote, modSrcVelocity, modSrcMonoLFO;

    ExciterParams exciterParams;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ResonariumProcessor)
};
