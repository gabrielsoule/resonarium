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

class ResonariumProcessor;

class ResonatorSynth : public gin::Synthesiser
{
public:
    explicit ResonatorSynth(ResonariumProcessor& p);
    void prepare(const juce::dsp::ProcessSpec& spec);
    void updateParameters();
    void renderNextSubBlock(juce::AudioBuffer<float>& outputAudio, int startSample, int numSamples) override;
    ResonariumProcessor& proc;
    gin::LFO monoLFOs[NUM_LFOS];
    RandomLFO monoRandomLFOs[NUM_RANDOMS];
    juce::Array<gin::MSEG> monoMSEGs;
    juce::Array<gin::MSEG::Data> msegData;
    SynthParams params;

    int currentBlockSize = -1;
};


#endif //RESONATORSYNTH_H
