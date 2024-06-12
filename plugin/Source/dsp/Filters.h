//
// Created by Gabriel Soule on 5/20/24.
//

#ifndef FILTERS_H
#define FILTERS_H
#include <JuceHeader.h>


/**
 * A first order allpass filter based on a lattice structure.
 *
 */
class DispersionFilter
{
public:
    float processSample(float input);
    void prepare(juce::dsp::ProcessSpec spec);
    void reset();
    void setDispersionAmount(float amount);

    float state[2];
    float c;
    float s;
};

/**
 * A one-zero zero-pole filter, i.e. a weighted two-sample average, as used in the original Karplus-Strong algorithm.
 * What could be simpler? Not much!
 * Perfection is achieved not when there is nothing more to add,
 * but when there is nothing left to take away. Or something like that.
 */
class OneZeroFilter
{
public:
    float processSample(float input);
    void prepare(juce::dsp::ProcessSpec spec);
    void reset();
    void setBrightness(float brightness);

    float state;
    float p;
};

#endif //FILTERS_H
