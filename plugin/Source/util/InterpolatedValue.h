#ifndef INTERPOLATEDVALUE_H
#define INTERPOLATEDVALUE_H

#include <JuceHeader.h>

/**
 * Smoothly interpolates between two values of a gin::Parameter over a specified number of samples.
 * For use within a single block of processing.
 */
class InterpolatedValue
{
public:
    InterpolatedValue()
    {
    }

    /**
     * Sets the target value of the InterpolatedParameter to the specified value,
     * using modulation from the specified source (either the monophonic ModMatrix or a polyphonic ModVoice).
     */
    void setTargetValue(float targetValue, int numSamples)
    {
        jassert(numSamples >= 0);
        if (numSamples == 0)
        {
            numSamples = 1;
        }
        if(resetFlag)
        {
            snapValue(targetValue);
        }
        target = targetValue;
        noop = current == target;
        increment = (target - current) / static_cast<float>(numSamples);
        samplesRemaining = numSamples;
    }

    /**
     * Snaps both the current and target values of the InterpolatedParameter to the specified source value,
     * and resets the filter state.
     */
    void snapValue(float value)
    {
        current = value;
        target = value;
        filterState = value;
    }

    //Resets the state of the InterpolatedParameter.
    //When setTargetValue is called for the first time after a reset,
    //the InterpolatedParameter will snap to the target value.
    void reset()
    {
        resetFlag = true;
    }

    /**
     * Performs a single step of the interpolation process, returning the next value of the InterpolatedParameter.
     * This function should only be called numSamples times before calling setTargetValue() again.
     */
    float nextValue()
    {
        jassert(samplesRemaining > 0);
        if (noop) return target;
        samplesRemaining--;
        current += increment;
        if (samplesRemaining == 0)
        {
            current = target;
        }
        return current;
    }

private:
    inline float processOnePole(float input)
    {
        filterState = input + onePoleCoefficient * (filterState - input);
        return filterState;
    }

    int samplesRemaining = -1;
    float filterState = 0;
    float current = 0;
    float target = 0;
    float increment = 0;
    bool noop = false;
    bool resetFlag;

    float onePoleCoefficient = 44100.0f - 44.0f / 44100.0f;
};


#endif //INTERPOLATEDVALUE_H
