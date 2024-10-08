//
// Created by Gabriel Soule on 5/1/24.
//

#ifndef RESONATORSYNTH_H
#define RESONATORSYNTH_H

#pragma once
#include "defines.h"
#include <JuceHeader.h>
#include "Parameters.h"
#include "util/RandomLFO.h"
#include "util/StereoLFOWrapper.h"
#include "util/StereoMSEGWrapper.h"

class ResonariumProcessor;

class ResonatorSynth : public gin::Synthesiser
{
public:
    ResonatorSynth(ResonariumProcessor& p);
    void setupParameters();
    void prepare(const juce::dsp::ProcessSpec& spec);
    void updateParameters();
    void renderNextSubBlock(juce::AudioBuffer<float>& outputAudio, int startSample, int numSamples) override;
    void panic();

    bool soloActive = false; //true if a resonator is in solo mode
    int soloBankIndex = 0; //the index of the solo resonator's enclosing bank
    int soloResonatorIndex = 0; //the resonator index itself

    ResonariumProcessor& proc;
    StereoLFOWrapper monoLFOs[NUM_LFOS];
    RandomLFO monoRandomLFOs[NUM_RANDOMS];
    juce::Array<StereoMSEGWrapper> monoMSEGs;
    juce::Array<gin::MSEG::Data> msegData;
    SynthParams params;

    int currentBlockSize = -1;
};


#endif //RESONATORSYNTH_H
