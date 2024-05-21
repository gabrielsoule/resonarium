//
// Created by Gabriel Soule on 5/1/24.
//

#ifndef RESONATOR_H
#define RESONATOR_H


#include <JuceHeader.h>

#include "Filters.h"

/**
 * A multi-purpose resonator with support for both waveguide models
 * and extended Karplus-Strong models.
 *
 * The Resonator object itself is designed to be used within
 * a ResonatorBank; therefore some variables are defined which
 * specifically apply to the enclosing ResonatorBank rather than the resonator itself.
 */
class Resonator {
public:


    enum Mode
    {
        WAVEGUIDE,
        Eks
    };

    struct HarmonicComponent
    {
        float harmonic;
        float gain;
        juce::dsp::IIR::Filter<float> dampingFilter;
    };

    /**
    *   Processes an entire sample through the resonator via computing the output sample after filtering and processing,
    *   pushing (input + output) into the delay line, and returning just the output sample.
    */
    float processSample(float input);

    /**
     * Pops a single sample from the resonator's delay line, applies waveguide filtering, and returns it.
     * MUST be followed by a call to pushSample.
     * This method allows more complex processing of the resonator's feedback input.
     */
    float popSample();
    float pushSample(float input);
    void reset();
    void prepare(const juce::dsp::ProcessSpec& spec);
    void setFrequency(float frequency);
    void setDampingFilterCutoff(float ßcutoff);
    void setDecayTime(float timeInSeconds);
    void setMode(Mode newMode);
    void setHarmonicMultiplier(float newHarmonicMultiplier);
    void setHarmonicOffsetInSemitones(float semitones, float cents);
    void updateParameters();

    juce::dsp::DelayLine<float> delayTop;
    juce::dsp::DelayLine<float> delayBtm;
    juce::dsp::IIR::Filter<float> dampingFilter;
    juce::dsp::IIR::Filter<float> dampingFilter2;
    DispersionFilter dispersionFilter;
    juce::dsp::IIR::Filter<float> dcBlocker;
    std::vector<HarmonicComponent> harmonics;
    Mode mode = Eks;
    float minFrequency; //the minimum frequency of the resonator
    float maxFrequency; //the maximum frequency of the resonator
    float delayLengthInSamples; //the length of the delay line in samples corresponding to frequency
    float decayCoefficient; //the first-order damping coefficient
    float decayTime;
    float dampingFilterCutoff; //the cutoff frequency of the damping filter
    float sampleRate;
    bool testMultiTap = false;

    //these parameters are managed by an enclosing ResonatorBank,
    //but they're stored in Resonator for simplicity
    float gain = 1.0f; //how loud should this resonator be?
    float feedbackGain = 1.0f; //how much should this resonator feed back into the resonator bank?
    float frequency; //the frequency of the resonator
    float harmonicMultiplier = 1.0f; //by how much should we multiply the base frequency

};




#endif //RESONATOR_H
