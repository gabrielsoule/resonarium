//
// Created by Gabriel Soule on 5/1/24.
//

#include "Resonator.h"
//TODO Support multi-channel processing and do some interesting stuff in the stereo field
float Resonator::processSample(float input)
{
    if (mode == Eks)
    {
        float outSample = 0;
        float outSample2 = 0;
        if(testMultiTap)
        {
            outSample2 = delayTop.popSample(0, delayLengthInSamples / 1.9f, false);
            outSample2 = outSample2 * dampingCoefficient;
            outSample2 = dampingFilter2.processSample(outSample2);
            outSample2 = outSample2 * 0.1f;
        }

        // implement Karplus-Strong algorithm using only the top delay line
        // delayTop.setDelay(delayLengthInSamples);
        outSample = outSample + delayTop.popSample(0, delayLengthInSamples, true);
        outSample = outSample * dampingCoefficient;
        outSample = dampingFilter.processSample(outSample);

        if(testMultiTap)
        {
            outSample = outSample +  outSample2;
            outSample = outSample / 1.1f;
        }
        delayTop.pushSample(0, outSample + input);
        return dcBlocker.processSample(outSample);
    }
    else if (mode == WAVEGUIDE)
    {
        DBG("Waveguide not yet implemented!");
        return -1;
    }
    else
    {
        DBG("Invalid resonator mode!");
        return -1;
    }
}

void Resonator::reset()
{
    delayTop.reset();
    delayBtm.reset();
    dampingFilter.reset();
    dampingFilter2.reset();
    dcBlocker.reset();
    testMultiTap = false;
    DBG("testMultiTap is now " + juce::String((testMultiTap ? "true" : "false")));
}

void Resonator::prepare(const juce::dsp::ProcessSpec& spec)
{
    this->reset();
    sampleRate = spec.sampleRate;
    minFrequency = 15;
    maxFrequency = (sampleRate / 2.0f) - 1;
    dampingFilterCutoff = spec.sampleRate / 4.0f;
    dampingCoefficient = 0.997f;
    delayTop.setMaximumDelayInSamples(4096);
    delayBtm.setMaximumDelayInSamples(4096);
    delayTop.prepare(spec);
    delayBtm.prepare(spec);

    dampingFilter.coefficients =
        juce::dsp::IIR::Coefficients<float>::makeFirstOrderLowPass(
        sampleRate, dampingFilterCutoff);
    dampingFilter.prepare(spec);

    dampingFilter2.coefficients = juce::dsp::IIR::Coefficients<float>::makeFirstOrderLowPass(
        sampleRate, dampingFilterCutoff);
    dampingFilter2.prepare(spec);

    //One pole DC blocker coefficients that are appropriate for a ~40-50hz sample rate
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
