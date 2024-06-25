//
// Created by Gabriel Soule on 5/1/24.
//

#ifndef RESONATOR_H
#define RESONATOR_H


#include <JuceHeader.h>

#include "Parameters.h"
#include "dsp/Filters.h"
#include "chowdsp_filters/LowerOrderFilters/chowdsp_StateVariableFilter.h"
#include "dsp/MultiFilter.h"

class ResonatorVoice;

/**
 * Encapsulates the different damping filters that can be used in the waveguide loop,
 * along with a set of hosted parameters.
 */
class WaveguideFilter
{
public:
    enum Type
    {
        //the classic order-two FIR filter used in the original Karplus-Strong algorithm
        //parameters: 1, cutoff 0.0f-1.0f
        eks,
        //a biquad filter with adjustable cutoff and resonance
        //parameters: (3), cutoff, resonance 0-100, type
        biquad,
        //a state-variable filter with adjustable cutoff and resonance
        //parameters: (3), cutoff, resonance 0-100, mode -1.0-1.0f
        svf,
        eq3, //parameters (4) (low, mid, high)
    };

    WaveguideFilter(ResonatorVoice& voice, ResonatorParams params);

    void prepare(const juce::dsp::ProcessSpec& spec);
    void reset();
    void updateParameters();
    float processSample(float sample);
    /**
    * Changes the type of filter used in the waveguide loop.
    * If the type differs from the current type, the filter is reset.
    * Otherwise, this is a no-op.
    */
    void setType(Type type);
    Type type = eks;
    MultiFilter biquadFilter;
    OneZeroFilter eksFilter;
    //SVF svfFilter; TODO implement
    //EQ3 eq3Filter; TODO implement

    ResonatorVoice& voice;
    ResonatorParams params;
};


/**
 * A multipurpose resonator based roughly on digital waveguide literature, including
 * the extended Karplus-Strong algorithm, banded waveguide synthesis, and other techniques.
 *
 * The Resonator object itself is designed to be used within
 * a ResonatorBank; therefore some variables are defined which
 * specifically apply to the enclosing ResonatorBank rather than the resonator itself.
 */
class Resonator
{
public:
    Resonator(ResonatorVoice& parentVoice, ResonatorParams params);

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
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> delayLine;

    float minFrequency; //the minimum frequency of the resonator
    float maxFrequency; //the maximum frequency of the resonator
    float delayLengthInSamples; //the length of the delay line in samples corresponding to frequency
    float decayCoefficient; //the first-order damping coefficient
    float decayTime;
    float sampleRate;

    WaveguideFilter loopFilter; //the filter used in the waveguide loop

    juce::dsp::IIR::Filter<float> dampingFilter;
    DispersionFilter dispersionFilter;
    OneZeroFilter oneZeroFilter;

    juce::dsp::IIR::Filter<float> dcBlocker;
    chowdsp::StateVariableFilter<float, chowdsp::StateVariableFilterType::MultiMode, 2> svf;

    //these parameters are managed by an enclosing ResonatorBank,
    //but they're stored in Resonator for simplicity
    float gain = 1.0f; //how loud should this resonator be? this is a scalar in [0, 1] -- not a -dB value!
    float feedbackGain = 1.0f; //how much should this resonator feed back into the resonator bank?
    float frequency; //the frequency of the resonator
    float harmonicMultiplier = 1.0f; //by how much should we multiply the base frequency

    //Pointer to the voice that owns this Resonator; awkwardly required for polyphonic modulation via ModMatrix
    ResonatorVoice& voice;

    //Pointers to the relevant parameters controlling this Resonator
    ResonatorParams params;

    //A general purpose test parameter used for debugging and A/B experimentation
    float testParameter;
    bool testFlag;
};


#endif //RESONATOR_H
