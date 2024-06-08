//
// Created by Gabriel Soule on 5/1/24.
//

#ifndef RESONATOR_H
#define RESONATOR_H


#include <JuceHeader.h>

#include "Parameters.h"
#include "dsp/Filters.h"
#include "chowdsp_filters/LowerOrderFilters/chowdsp_StateVariableFilter.h"

class ResonatorVoice;
/**
 * A multi-purpose resonator based roughly on digital waveguide literature, including
 * the extended Karplus-Strong algorithm, banded waveguide synthesis, and other techniques.
 *
 * The Resonator object itself is designed to be used within
 * a ResonatorBank; therefore some variables are defined which
 * specifically apply to the enclosing ResonatorBank rather than the resonator itself.
 */
class Resonator
{
public:
    Resonator(ResonatorVoice& parentVoice);

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
    // void setFrequency(float frequency);
    // void setDampingFilterCutoff(float cutoff);
    // void setDecayTime(float timeInSeconds);
    // void setHarmonicMultiplier(float newHarmonicMultiplier);
    void setHarmonicOffsetInSemitones(float semitones, float cents);

    /**
     * Computes and caches internal parameters based on the
     * raw values of the VST3 parameter pointers. Called once
     * per block for efficiency.
     */
    void updateParameters(float frequency);

    bool enabled;
    juce::dsp::DelayLine<float> delayLine;
    juce::dsp::IIR::Filter<float> dampingFilter;
    DispersionFilter dispersionFilter;
    juce::dsp::IIR::Filter<float> dcBlocker;
    float minFrequency; //the minimum frequency of the resonator
    float maxFrequency; //the maximum frequency of the resonator
    float delayLengthInSamples; //the length of the delay line in samples corresponding to frequency
    float decayCoefficient; //the first-order damping coefficient
    float decayTime;
    float sampleRate;

    chowdsp::StateVariableFilter<float, chowdsp::StateVariableFilterType::MultiMode, 2> svf;

    //these parameters are managed by an enclosing ResonatorBank,
    //but they're stored in Resonator for simplicity
    float gain = 1.0f; //how loud should this resonator be? this is a scalar in [0, 1] -- not a -dB value!
    float feedbackGain = 1.0f; //how much should this resonator feed back into the resonator bank?
    float frequency; //the frequency of the resonator
    float harmonicMultiplier = 1.0f; //by how much should we multiply the base frequency

    //Pointers to the relevant parameters controlling this Resonator
    ResonatorParams params;

    //Pointer to the voice that owns this Resonator; awkwardly required for polyphonic modulation via ModMatrix
    ResonatorVoice& voice;

    //A general purpose test parameter used for debugging and A/B experimentation
    float testParameter;
    bool testFlag;
};


#endif //RESONATOR_H
