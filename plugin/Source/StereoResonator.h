//
// Created by Gabriel Soule on 7/22/24.
//

#ifndef STEREORESONATOR_H
#define STEREORESONATOR_H

#include <JuceHeader.h>

#include "Parameters.h"
#include "dsp/Filters.h"
#include "util/InterpolatedParameter.h"
#include "util/InterpolatedValue.h"

class ResonatorVoice;

class StereoResonator
{
    class Resonator
    {
    public:
        Resonator(ResonatorVoice& voice, ResonatorParams params, int channel) : voice(voice), params(params),
                                                                               channel(channel), delayLine(21000)
        {
            jassert(channel == 0 || channel == 1);
        }

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
        /**
         * Push a sample into the delay line.
         * An invocation of this function should generally be preceeded by an invocation of popSample().
         */
        void pushSample(float input);

        float postProcess(float sample);

        void reset();
        void prepare(const juce::dsp::ProcessSpec& spec);
        void updateParameters(float frequency, int numSamples);
        void copyParameters(StereoResonator::Resonator& other);

        ResonatorVoice& voice;
        ResonatorParams params;
        int channel = 0;
        float minFrequency = 1.0f;
        float maxFrequency = 22000.0f;
        float delayLengthInSamples; //the length of the delay line in samples corresponding to frequency
        float decayCoefficient; //the first-order damping coefficient
        float decayTime;
        float sampleRate;

        bool passthrough = false;
        float passthroughSample;

        float gain;
        bool keytrack = true;
        float lastFrequency;
        float nextFrequency;
        float dispersion;
        bool loopFilterKeytrack = false;
        float loopFilterCutoff;
        float loopFilterResonance;
        float loopFilterMode;
        float loopFilterNormalizationScalar = 1; //the inverse of the max gain point of the multi-mode SVF
        bool postFilterKeytrack = false;
        float postFilterCutoff;
        float postFilterResonance;
        float postFilterMode;
        float postFilterNormalizationScalar = 1;

        InterpolatedValue delayLengthInterpolator;

        chowdsp::DelayLine<float, chowdsp::DelayLineInterpolationTypes::Lagrange3rd> delayLine;
        chowdsp::SVFMultiMode<float, 1, true> loopFilter;
        chowdsp::SVFMultiMode<float, 1, false> postFilter;
        DispersionFilter apf;
    };

public:
    StereoResonator(ResonatorVoice& voice, ResonatorParams params)
        : voice(voice), params(params),
          resonators{{voice, params, 0}, {voice, params, 1}}, left(resonators[0]), right(resonators[1])
    {
    }

    ResonatorVoice& voice;
    ResonatorParams params;
    Resonator resonators[2];
    Resonator& left; //handy aliases
    Resonator& right;
    bool enabled;

    float processSample(float input, int channel);

    /**
     * Pops a single sample from the resonator's delay line, applies waveguide filtering, and returns it.
     * MUST be followed by a call to pushSample.
     * This method allows more complex processing of the resonator's feedback input.
     */
    float popSample(int channel);
    /**
     * Push a sample into the delay line.
     * An invocation of this function should generally be preceeded by an invocation of popSample().
     */
    void pushSample(float input, int channel);
    float postProcess(float sample, int channel);
    void reset();
    void prepare(const juce::dsp::ProcessSpec& spec);
    void updateParameters(float frequency, int numSamples, bool force = false);
};


#endif //STEREORESONATOR_H
