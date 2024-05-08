//
// Created by Gabriel Soule on 5/1/24.
//

#ifndef RESONATOR_H
#define RESONATOR_H


#include <JuceHeader.h>

/**
 * A multi-purpose resonator with support for both waveguide models
 * and extended Karplus-Strong models.
 */
class Resonator {
public:

    enum Mode
    {
        WAVEGUIDE,
        Eks
    };

    float processSample(float input);
    void reset();
    void prepare(const juce::dsp::ProcessSpec& spec);
    void setFrequency(float frequency);
    void setDampingFilterCutoff(float cutoff);
    void setMode(Mode newMode);

    juce::dsp::DelayLine<float> delayTop;
    juce::dsp::DelayLine<float> delayBtm;
    juce::dsp::IIR::Filter<float> dampingFilter;
    juce::dsp::IIR::Filter<float> dcBlocker;
    Mode mode;
    float frequency;
    float minFrequency;
    float maxFrequency;
    float delayLengthInSamples;
    float dampingCoefficient;
    float dampingFilterCutoff;
    float sampleRate;
};




#endif //RESONATOR_H
