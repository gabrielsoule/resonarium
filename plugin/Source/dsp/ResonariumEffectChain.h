//
// Created by Gabriel Soule on 8/2/24.
//

#ifndef RESONARIUMEFFECTCHAIN_H
#define RESONARIUMEFFECTCHAIN_H

#include <JuceHeader.h>

#include "Distortion.h"
#include "MultiFilter.h"

class ResonariumEffectChain {

    template <typename T>
    void updateParameters(T source, float frequency)
    {

    }

    juce::dsp::Chorus<float> chorus;
    juce::dsp::Reverb reverb;
    gin::StereoDelay delay;
    juce::dsp::Gain<float> gain;
    juce::dsp::Phaser<float> phaser;
    MultiFilter filter1;
    MultiFilter filter2;
    Distortion distortion;
};



#endif //RESONARIUMEFFECTCHAIN_H
