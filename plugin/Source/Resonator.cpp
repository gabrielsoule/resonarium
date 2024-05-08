//
// Created by Gabriel Soule on 5/1/24.
//

#include "Resonator.h"

//TODO Support multi-channel processing and do some interesting stuff in the stereo field
float Resonator::processSample(float input)
{
    if (mode == Eks)
    {
        // implement Karplus-Strong algorithm using only the top delay line
        float outSample = delayTop.popSample(0);
        outSample = outSample * dampingCoefficient;
        outSample = dampingFilter.processSample(outSample);
        delayTop.pushSample(0, outSample + input);
        return dcBlocker.processSample(outSample);
    }
    else if (mode == WAVEGUIDE)
    {
        DBG("Waveguide not yet implemented!");
    }
    else
    {
        DBG("Invalid resonator mode!");
    }
}

void Resonator::reset()
{
    delayTop.reset();
    delayBtm.reset();
    dampingFilter.reset();
    dcBlocker.reset();
}

void Resonator::prepare(const juce::dsp::ProcessSpec& spec)
{
    this->reset();
    sampleRate = spec.sampleRate;
    minFrequency = 15;
    maxFrequency = (sampleRate / 2.0f) - 1;
    dampingFilterCutoff = spec.sampleRate / 4.0f;
    dampingCoefficient = 0.999f;
    delayTop.setMaximumDelayInSamples(4096);
    delayBtm.setMaximumDelayInSamples(4096);
    delayTop.prepare(spec);
    delayBtm.prepare(spec);

    dampingFilter.coefficients =
        juce::dsp::IIR::Coefficients<float>::makeFirstOrderLowPass(
        sampleRate, dampingFilterCutoff);
    dampingFilter.prepare(spec);

    juce::dsp::IIR::Coefficients<float>::Ptr dcBlockerCoefficients =
        new juce::dsp::IIR::Coefficients<float>(1, -1, 1, -0.995f);
    dcBlocker.coefficients = dcBlockerCoefficients;
    dcBlocker.prepare(spec);
}

void Resonator::setFrequency(float newFrequency)
{
    frequency = newFrequency;
    //convert the frequency to the corresponding delay length
    delayLengthInSamples = sampleRate / frequency;
    // DBG("Setting delay length to " + juce::String(delayLengthInSamples) + " samples");
    delayTop.setDelay(delayLengthInSamples);
    delayBtm.setDelay(delayLengthInSamples);
}

void Resonator::setDampingFilterCutoff(float cutoff)
{
    if (cutoff != dampingFilterCutoff)
    {
        dampingFilterCutoff = cutoff;
        dampingFilter.coefficients = juce::dsp::IIR::Coefficients<float>::makeFirstOrderLowPass(
            sampleRate, dampingFilterCutoff);
    }
}

void Resonator::setMode(Mode newMode)
{
    mode = newMode;
}
