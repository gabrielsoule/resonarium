//
// Created by Gabriel Soule on 5/1/24.
//

#include "Resonator.h"
//TODO Support multi-channel processing and do some interesting stuff in the stereo field
/**
 * WARNING: Since the enclosing ResonatorBank manages the feedback gain and output gain of each resonator,
 * essentially duplicating the signal, the resonator does NOT apply its own gain to its own output signal.
 * The gain must be applied by the caller of processSample.
 * @param input
 * @return
 */
float Resonator::processSample(float input)
{
    // if (mode == Eks)
    // {
    //     float outSample = 0;
    //     // float outSample2 = 0;
    //     // if(testMultiTap)
    //     // {
    //     //     outSample2 = delayTop.popSample(0, delayLengthInSamples / 1.9f, false);
    //     //     outSample2 = outSample2 * dampingCoefficient;
    //     //     outSample2 = dampingFilter2.processSample(outSample2);
    //     //     outSample2 = outSample2 * 0.1f;
    //     // }
    //
    //     // implement Karplus-Strong algorithm using only the top delay line
    //     // delayTop.setDelay(delayLengthInSamples);
    //     outSample = outSample + delayTop.popSample(0, delayLengthInSamples, true);
    //     outSample = dampingFilter.processSample(outSample);
    //     outSample = dispersionFilter.processSample(outSample);
    //     outSample = outSample * decayCoefficient;
    //
    //
    //     // if(testMultiTap)
    //     // {
    //     //     outSample = outSample +  outSample2;
    //     //     outSample = outSample / 1.1f;
    //     // }
    //     delayTop.pushSample(0, outSample + input);
    //     return dcBlocker.processSample(outSample);
    // }
    // else if (mode == WAVEGUIDE)
    // {
    //     DBG("Waveguide not yet implemented!");
    //     return -1;
    // }
    // else
    // {
    //     DBG("Invalid resonator mode!");
    //     return -1;
    // }
    float outSample = popSample();
    pushSample(outSample + input);
    return outSample;
}

float Resonator::popSample()
{
    float outSample = 0;
    outSample = outSample + delayTop.popSample(0, delayLengthInSamples, true);
    outSample = dampingFilter.processSample(outSample);
    outSample = dispersionFilter.processSample(outSample);
    outSample = outSample * decayCoefficient;
    return outSample;
}

float Resonator::pushSample(float input)
{
    delayTop.pushSample(0, dcBlocker.processSample(input));
    return input;
}


void Resonator::reset()
{
    delayTop.reset();
    delayBtm.reset();
    dampingFilter.reset();
    dampingFilter2.reset();
    dcBlocker.reset();
    dispersionFilter.reset();
    testMultiTap = false;
}

void Resonator::prepare(const juce::dsp::ProcessSpec& spec)
{
    this->reset();
    this->frequency = 440.0f; //set the frequency to anything just to avoid dividing by zero when computing delay length
    sampleRate = spec.sampleRate;
    minFrequency = 15;
    maxFrequency = (sampleRate / 2.0f) - 1;
    dampingFilterCutoff = spec.sampleRate / 4.0f;
    setDecayTime(2.0f);
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

    dispersionFilter.setDispersionAmount(0.0f);
    dispersionFilter.prepare(spec);

    //One pole DC blocker coefficients that are appropriate for a ~40-50hz sample rate
    juce::dsp::IIR::Coefficients<float>::Ptr dcBlockerCoefficients =
        new juce::dsp::IIR::Coefficients<float>(1, -1, 1, -0.995f);
    dcBlocker.coefficients = dcBlockerCoefficients;
    dcBlocker.prepare(spec);
    updateParameters();
}

void Resonator::setFrequency(float newFrequency)
{
    frequency = newFrequency;
    updateParameters();

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

void Resonator::setHarmonicMultiplier(float newHarmonicMultiplier)
{
    harmonicMultiplier = newHarmonicMultiplier;
    updateParameters();
}

/**
 * Applies a frequency offset to this resonator's frequency, based on a number of semitones and cents.
 */
void Resonator::setHarmonicOffsetInSemitones(float semitones, float cents)
{
    //convert the semitones and cents into a frequency offset, based on the current frequency
    this->harmonicMultiplier =  std::pow(2.0, semitones / 12.0 + cents / 1200.0);
    updateParameters();
}

/**
 * Updates all frequency-dependent parameters of the resonator.
 * Called internally after, e.g. a frequency change.
 */
void Resonator::updateParameters()
{
    delayLengthInSamples = sampleRate / (frequency * harmonicMultiplier);
    delayTop.setDelay(delayLengthInSamples);
    delayBtm.setDelay(delayLengthInSamples);
    // DBG("Setting delay length to " + juce::String(delayLengthInSamples) + " samples corresponding to a harmonic multiplier of " + juce::String(harmonicMultiplier) + " and a frequency of " + juce::String(frequency) + " Hz.");
}

void Resonator::setDecayTime(float timeInSeconds)
{
    decayTime = timeInSeconds;
    decayCoefficient = std::pow(0.001, 1.0 / (timeInSeconds * frequency));
    DBG("Setting decay coefficient to " + juce::String(decayCoefficient) + " for a decay time of " + juce::String(decayCoefficient));
}
