//
// Created by Gabriel Soule on 5/1/24.
//

#ifndef RESONATORSYNTH_H
#define RESONATORSYNTH_H

#pragma once
#include "defines.h"
#include <JuceHeader.h>

#include "GlobalState.h"
#include "Parameters.h"
#include "dsp/ResonariumEffectChain.h"
#include "util/RandomLFO.h"
#include "util/StereoLFOWrapper.h"
#include "util/StereoMSEGWrapper.h"

class ResonatorSynth : public gin::Synthesiser
{
public:
    ResonatorSynth(GlobalState& state, SynthParams params);
    void prepare(const juce::dsp::ProcessSpec& spec);
    void updateParameters();
    void renderNextSubBlock(juce::AudioBuffer<float>& outputAudio, int startSample, int numSamples) override;
    void panic();

    GlobalState& state;
    SynthParams params;
    ResonariumEffectChain effectChain;
    StereoLFOWrapper monoLFOs[NUM_LFOS];
    RandomLFO monoRandomLFOs[NUM_RANDOMS];
    juce::Array<StereoMSEGWrapper> monoMSEGs;
    juce::Array<gin::MSEG::Data> msegData;

    int currentBlockSize = -1;
};


#endif //RESONATORSYNTH_H
