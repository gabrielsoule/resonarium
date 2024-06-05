//
// Created by Gabriel Soule on 5/20/24.
//

#include "Filters.h"

#include "juce_core/unit_tests/juce_UnitTestCategories.h"

float DispersionFilter::processSample(float input)
{
    // Calculate the current output sample using the allpass filter formula
    state[0] = c * input - s * state[1];
    const auto output = s * input + c * state[1];
    state[1] = state[0];
    return output;
}

void DispersionFilter::prepare(juce::dsp::ProcessSpec spec)
{
    //do nothing, filter does not need to know sample rate
}

/**
 * Reset the internal state variables of the filter.
 */
void DispersionFilter::reset()
{
    state[0] = 0;
    state[1] = 0;
}

void DispersionFilter::setDispersionAmount(float amount)
{
    float angle = amount * -juce::MathConstants<float>::halfPi;
    c = std::cos(angle);
    s = std::sin(angle);
    DBG("Dispersion filter coefficients: c = " + juce::String(c) + ", s = " + juce::String(s));
}
