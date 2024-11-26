#include "StereoResonator.h"
#include "ResonatorVoice.h"

float StereoResonator::Resonator::processSample(float input)
{
    if (passthrough)
    {
        return input;
    }
    float outSample = popSample();
    pushSample(outSample + input);
    return outSample;
}

float StereoResonator::Resonator::popSample()
{
    if (passthrough)
    {
        return passthroughSample;
    }
    auto delay = delayLengthInterpolator.nextValue();
    delayLine.setDelay(delay);
    float outSample = delayLine.popSample(0);
    outSample = loopFilter.processSample(0, outSample);
    outSample = apf.processSample(outSample);
    return outSample * decayCoefficient;
}

void StereoResonator::Resonator::pushSample(float input)
{
    if (passthrough)
    {
        passthroughSample = input;
        return;
    }
    delayLine.pushSample(0, input);
}

float StereoResonator::Resonator::postProcess(float input)
{
    const float sample = postFilter.processSample(0, input);
    // float sample = input;
    return sample;
}

void StereoResonator::Resonator::reset()
{
    delayLengthInterpolator.snapValue(delayLengthInSamples);
    delayLine.reset();
    loopFilter.reset();
    postFilter.reset();
    apf.reset();
    // auto span = delayLine.getRawDelayBuffer().getWriteSpan(0);
    // for (auto& sample : span)
    // {
    //     sample = 0.0f;
    // }
#ifdef JUCE_DEBUG
    auto readSpan = delayLine.getRawDelayBuffer().getReadSpan(0);
    for (auto sample : readSpan)
    {
        jassert(sample == 0.0f);
    }
    if (delayLengthInSamples < 0)
    {
        //reset called before prepare and update
        DBG("Resonator reset before prepare and update. This is likely unintended and may cause problems.");
        jassertfalse;
    }
#endif

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

//This function is the worst. It always ends up being the worst. Just a mess.
//I've already re-written the entire class from scratch once because it was the worst then too.
//"Let it not be the worst this time," I proclaimed.
//But now it's the worst again. Somehow.
//TODO Study why this keeps happening to me.
void StereoResonator::Resonator::updateParameters(float frequency, int numSamples)
{
    this->gain = voice.getValue(params.gain);

    postFilterKeytrack = params.postFilterKeytrack->isOn();
    const float newPostFilterCutoff = postFilterKeytrack
                                          ? frequency * std::pow(
                                              2.0f, voice.getValue(params.postFilterPitchInSemis, channel) / 12.0f)
                                          : voice.getValue(params.postFilterCutoff, channel);
    const float newPostFilterResonance = voice.getValue(params.postFilterResonance, channel);
    const float newPostFilterMode = voice.getValue(params.postFilterMode, channel);
    postFilter.updateParameters(newPostFilterCutoff, newPostFilterResonance, newPostFilterMode);
    const float decayInSeconds = voice.getValue(params.decayTime, channel);

    if (decayInSeconds < 0.03f)
    {
        passthrough = true;
        return;
    }
    passthrough = false;
    passthroughSample = 0;
    lastFrequency = nextFrequency;
    keytrack = params.resonatorKeytrack->isOn();
    if (keytrack)
    {
        nextFrequency = voice.getValue(params.pitch) * frequency;
    }
    else
    {
        nextFrequency = voice.getValue(params.frequency, channel);
    }

    if (decayInSeconds == 60.0f)
    {
        decayCoefficient = 1.0f;
    }
    else
    {
        decayCoefficient = std::pow(0.001f, 1.0f / (decayInSeconds * nextFrequency));
    }

    loopFilterKeytrack = params.loopFilterKeytrack->isOn();
    const float newCutoff = loopFilterKeytrack
                                ? nextFrequency * std::pow(
                                    2.0f, voice.getValue(params.loopFilterPitchInSemis, channel) / 12.0f)
                                : voice.getValue(params.loopFilterCutoff, channel);
    const float newResonance = voice.getValue(params.loopFilterResonance, channel) + 0.001f;
    const float newMode = params.loopFilterType->getProcValue() * 0.5f;
    jassert(newMode == 0 || newMode == 0.5 || newMode == 1);
    bool updated = loopFilter.updateParameters(newCutoff, newResonance, newMode, loopFilterKeytrack);
    if (updated)
    {
        if(newMode == 0.5)
        {
            loopFilterPhaseDelay = 0;
        } else
        {
            loopFilterPhaseDelay = loopFilter.getPhaseDelayInSamples(nextFrequency);
        }
    }

    float newDispersion = voice.getValue(params.dispersion, channel);
    dispersion = newDispersion;
    apf.setDispersionAmount(newDispersion);

    //experimentally, the delay line Lagrange interpolation adds a delay of one sample plus change.
    //this tuning error, if unaddressed, is readily observed at higher frequencies.
    constexpr float DELAY_LINE_INTERPOLATION_DELAY = 1.03f;
    delayLengthInSamples = sampleRate / nextFrequency - DELAY_LINE_INTERPOLATION_DELAY;
    delayLengthInSamples = delayLengthInSamples - loopFilterPhaseDelay;
    if(delayLengthInSamples < 2.0f)
    {
        DBG("WARNING: Delay length in samples is too small, setting to 2.0f");
        DBG("Attempted delay length: " << delayLengthInSamples);
        delayLengthInSamples = 2.0f;
    }
    delayLengthInterpolator.setTargetValue(delayLengthInSamples, numSamples);
    delayLine.setDelay(delayLengthInSamples);

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
    updateParameters(440.0f, spec.maximumBlockSize, true);
}

void StereoResonator::updateParameters(float frequency, int numSamples, bool force)
{
    this->enabled = params.enabled->isOn();
    if (this->enabled || force)
    {
        resonators[0].updateParameters(frequency, numSamples);
        resonators[1].updateParameters(frequency, numSamples);
    }
    else
    {
        resonators[0].gain = 0;
        resonators[1].gain = 0;
    }
}
