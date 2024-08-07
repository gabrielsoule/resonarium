#include "StereoResonator.h"
#include "ResonatorVoice.h"

float StereoResonator::Resonator::processSample(float input)
{
    float outSample = popSample();
    pushSample(outSample + input);
    return outSample;
}

float StereoResonator::Resonator::popSample()
{
    auto delay = delayLengthInterpolator.nextValue();
    // DBG(delay);
    delayLine.setDelay(delay);
    float outSample = delayLine.popSample(0);
    outSample = loopFilter.processSample(0, outSample) * loopFilterNormalizationScalar;
    outSample = apf.processSample(outSample);
    return outSample * decayCoefficient;
}

void StereoResonator::Resonator::pushSample(float input)
{
    delayLine.pushSample(0, input);
}

inline float StereoResonator::Resonator::postProcess(float input)
{
    const float sample = postFilter.processSample(0, input);
    // float sample = input;
    return sample;
}

void StereoResonator::Resonator::reset()
{
    delayLine.reset();
    delayLengthInterpolator.snapValue(delayLengthInSamples);
    loopFilter.reset();
    postFilter.reset();
    apf.reset();
    auto span = delayLine.getRawDelayBuffer().getWriteSpan(0);
    for(auto& sample : span)
    {
        sample = 0.0f;
    }

}

void StereoResonator::Resonator::prepare(const juce::dsp::ProcessSpec& spec)
{
    this->sampleRate = spec.sampleRate;
    loopFilter.prepare({spec.sampleRate, spec.maximumBlockSize, 1});
    postFilter.prepare({spec.sampleRate, spec.maximumBlockSize, 1});
    apf.prepare(spec);
    delayLine.prepare(spec);
    this->reset();
}

void StereoResonator::Resonator::updateParameters(float frequency, int numSamples)
{
    this->gain = voice.getValue(params.gain);
    lastFrequency = nextFrequency;
    nextFrequency = frequency * std::pow(2.0f, voice.getValue(params.harmonicInSemitones, channel) / 12.0f);
    delayLengthInSamples = sampleRate / nextFrequency;
    delayLengthInterpolator.setTargetValue(delayLengthInSamples, numSamples);
    delayLine.setDelay(delayLengthInSamples);
    const float decayInSeconds = voice.getValue(params.decayTime, channel);
    if(decayInSeconds == 60.0f)
    {
        decayCoefficient = 1.0f;
    }
    else
    {
        decayCoefficient = std::pow(0.001f, 1.0f / (decayInSeconds * nextFrequency));
    }

    float newCutoff = voice.getValue(params.loopFilterCutoff, channel);
    float newResonance = voice.getValue(params.loopFilterResonance, channel);
    float newMode = voice.getValue(params.loopFilterMode, channel);
    // if(newCutoff != loopFilterCutoff || newResonance != loopFilterResonance || newMode != loopFilterMode)
    // {
    //     loopFilterResonance = newResonance;
    //     loopFilterCutoff = newCutoff;
    //     loopFilterMode = newMode;
    //     loopFilter.setCutoffFrequency<false>(newCutoff);
    //     loopFilter.setQValue<false>(newResonance);
    //     loopFilter.setMode(newMode);
    //     loopFilter.update();
    //     // loopFilterNormalizationScalar = 1.0f / loopFilter.getMultiModeMaxGain();
    // }
    // if(this->voice.id > -1)
    // {
    //     DBG("Updating parameters for " + juce::String(this->voice.id));
    //     DBG("Cutoff: " + juce::String(newCutoff));
    //     DBG("Resonance: " + juce::String(newResonance));
    //     DBG("Mode: " + juce::String(newMode));
    // }
    loopFilter.updateParameters(newCutoff, newResonance, newMode);

    newCutoff = voice.getValue(params.postFilterCutoff, channel);
    newResonance = voice.getValue(params.postFilterResonance, channel);
    newMode = voice.getValue(params.postFilterMode, channel);
    // if(newCutoff != postFilterCutoff || newResonance != postFilterResonance || newMode != postFilterMode)
    // {
    //     postFilterResonance = newResonance;
    //     postFilterCutoff = newCutoff;
    //     postFilterMode = newMode;
    //     postFilter.setCutoffFrequency<false>(newCutoff);
    //     postFilter.setQValue<false>(newResonance);
    //     postFilter.setMode(newMode);
    //     postFilter.update();
    //     // postFilterNormalizationScalar = 1.0f / postFilter.getMultiModeMaxGain();
    // }
    postFilter.updateParameters(newCutoff, newResonance, newMode);

    float newDispersion = voice.getValue(params.dispersion, channel);
    // if(newDispersion != dispersion)
    // {
        dispersion = newDispersion;
        apf.setDispersionAmount(newDispersion);
    // }

#if JUCE_SNAP_TO_ZERO
    postFilter.snapToZero();
    loopFilter.snapToZero();
#endif
}

/**
 * Copy the raw parameter values from another Resonator, skipping all the intermediate calculations.
 * Useful when there are no stereo modulation signals affecting this StereoResonator.
 */
// void StereoResonator::Resonator::copyParameters(StereoResonator::Resonator& other)
// {
//     this->gain = other.gain;
//     this->lastFrequency = other.lastFrequency;
//     this->nextFrequency = other.nextFrequency;
//     this->delayLengthInSamples = other.delayLengthInSamples;
//     delayLine.setDelay(delayLengthInSamples);
//
// }

float StereoResonator::processSample(const float input, const int channel)
{
    jassert(channel == 0 || channel == 1);
    if (!enabled) return 0.0f;
    return resonators[channel].processSample(input);
}

float StereoResonator::popSample(int channel)
{
    jassert(channel == 0 || channel == 1);
    if (!enabled) return 0.0f;
    return resonators[channel].popSample();
}

void StereoResonator::pushSample(float input, int channel)
{
    jassert(channel == 0 || channel == 1);
    if (!enabled) return;
    resonators[channel].pushSample(input);
}

float StereoResonator::postProcess(float sample, int channel)
{
    jassert(channel == 0 || channel == 1);
    if (!enabled) return 0.0f;
    return resonators[channel].postProcess(sample);
}

void StereoResonator::reset()
{
    resonators[0].reset();
    resonators[1].reset();
}

void StereoResonator::prepare(const juce::dsp::ProcessSpec& spec)
{
    resonators[0].prepare(spec);
    resonators[1].prepare(spec);
    updateParameters(440.0f, true);
}

void StereoResonator::updateParameters(float frequency, int numSamples, bool force)
{
    this->enabled = params.enabled->isOn();
    if(this->enabled || force)
    {
        resonators[0].updateParameters(frequency, numSamples);
        resonators[1].updateParameters(frequency, numSamples);

    } else
    {
        resonators[0].gain = 0;
        resonators[1].gain = 0;
    }
}
