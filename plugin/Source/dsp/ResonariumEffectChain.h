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

    void process (juce::AudioBuffer<float>& buffer) noexcept
    {
        juce::dsp::AudioBlock<float> block = juce::dsp::AudioBlock<float> (buffer);
        juce::dsp::ProcessContextReplacing context (block);
        chorus.process(context);
    }

    juce::dsp::Chorus<float> chorus;
    gin::StereoDelay delay;
    Distortion distortion;
    MultiFilter filter1;
    MultiFilter filter2;
    juce::dsp::Phaser<float> phaser;
    juce::dsp::Reverb reverb;
    juce::dsp::Gain<float> gain;
};



#endif //RESONARIUMEFFECTCHAIN_H
