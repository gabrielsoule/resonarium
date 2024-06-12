//
// Created by Gabriel Soule on 5/20/24.
//

#include "Filters.h"

float DispersionFilter::processSample(float input)
{
    state[0] = c * input - s * state[1];
    const auto output = s * input + c * state[1];
    state[1] = state[0];
    return output;
}

void DispersionFilter::prepare(juce::dsp::ProcessSpec spec)
{
    //no-op for now, filter does not need to know sample rate
}

void DispersionFilter::reset()
{
    state[0] = 0;
    state[1] = 0;
}

/**
 * Sets the amount of dispersion to apply to the signal (when placed inside a waveguide loop).
 * 0 = no dispersion, 1 = maximum dispersion, i.e. a negative polarity comb filter.
 */
void DispersionFilter::setDispersionAmount(float amount)
{
    amount = juce::jlimit(0.0f, 1.0f, amount);
    float angle = amount * -juce::MathConstants<float>::halfPi;
    c = std::cos(angle);
    s = std::sin(angle);
}

float OneZeroFilter::processSample(float input)
{
    const float output = (1 - p) * input + p * state;
    state = output;
    return output;
}

void OneZeroFilter::prepare(juce::dsp::ProcessSpec spec)
{

}

void OneZeroFilter::reset()
{
    state = 0;
}

void OneZeroFilter::setBrightness(float brightness)
{
    this->p = brightness;
}



