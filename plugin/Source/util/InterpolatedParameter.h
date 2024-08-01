#ifndef INTERPOLATEDPARAMETER_H
#define INTERPOLATEDPARAMETER_H

#include <JuceHeader.h>

/**
 * Smoothly interpolates between two values of a gin::Parameter over a specified number of samples.
 * For use within a single block of processing.
 */
class InterpolatedParameter
{
public:

    explicit InterpolatedParameter(gin::Parameter::Ptr parameter) : parameter(parameter)
    {
        current[0] = parameter->getProcValue();
        current[1] = parameter->getProcValue();
        target[0] = current[0];
        target[1] = current[1];
        filterState[0] = current[0];
        filterState[1] = current[1];
        samplesRemaining = -1;
        increment[0] = 0;
        increment[1] = 0;
    }

    /**
     * Sets the target value of the InterpolatedParameter to the specified value,
     * using modulation from the specifed source (either the monophonic ModMatrix or a polyphonic ModVoice).
     */
    template <typename T>
    void setTargetValue(T source, int numSamples)
    {
        jassert(numSamples >= 0);
        if(numSamples == 0)
        {
            numSamples = 1;
        }
        target[0] = source.getValue(parameter, 0);
        target[1] = source.getValue(parameter, 1);
        if(current[0] == target[0])
        {
            noop[0] = true;
        }
        if(current[1] == target[1])
        {
            noop[1] = true;
        }
        increment[0] = (target[0] - current[0]) / static_cast<float>(numSamples);
        increment[1] = (target[1] - current[1]) / static_cast<float>(numSamples);
        samplesRemaining = numSamples;
    }

    /**
     * Snaps both the current and target values of the InterpolatedParameter to the specified source value,
     * and resets the filter state.
     */
    template <typename T>
    void snapValue(T source)
    {
        current[0] = source.getValue(parameter, 0);
        current[1] = source.getValue(parameter, 1);
        target[0] = current[0];
        target[1] = current[1];
        filterState[0] = current[0];
        filterState[1] = current[1];
    }

    /**
     * Performs a single step of the interpolation process, returning the next value of the InterpolatedParameter.
     * This function should only be called numSamples times before calling setTargetValue() again.
     */
    float nextValue(int channel = 0)
    {
        jassert(channel == 0 || channel == 1);
        jassert(samplesRemaining > 0);
        samplesRemaining--;
        current[channel] += increment[channel];
        if(samplesRemaining == 0)
        {
            current[channel] = target[channel];
        }
        return processOnePole(current[channel], channel);
    }

private:

    inline float processOnePole(float input, int channel = 0)
    {
        filterState[channel] = input + onePoleCoefficient * (filterState[channel] - input);
        return filterState[channel];
    }

    gin::Parameter::Ptr parameter;
    int samplesRemaining;
    float filterState[2];
    float current[2];
    float target[2];
    float increment[2];
    bool noop[2];

    float onePoleCoefficient = 44100.0f - 44.0f / 44100.0f;
};


#endif //INTERPOLATEDPARAMETER_H
