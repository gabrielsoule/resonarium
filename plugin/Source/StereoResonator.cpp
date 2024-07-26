#include "StereoResonator.h"
#include "ResonatorVoice.h"

float StereoResonator::Resonator::processSample(float input)
{
    float outSample = popSample();
    pushSample(outSample + input);
    // DBG(outSample);
    return outSample;
}

float StereoResonator::Resonator::popSample()
{
    float outSample = delayLine.popSample(0);
    outSample = svf.processSample(0, outSample);
    outSample = apf.processSample(outSample);
    return outSample * decayCoefficient;
}

void StereoResonator::Resonator::pushSample(float input)
{
    delayLine.pushSample(0, input);
}

void StereoResonator::Resonator::reset()
{
    delayLine.reset();
    svf.reset();
    apf.reset();
}

void StereoResonator::Resonator::prepare(const juce::dsp::ProcessSpec& spec)
{
    this->reset();
    this->sampleRate = spec.sampleRate;
    svf.prepare({spec.sampleRate, spec.maximumBlockSize, 1});
    apf.prepare(spec);
    delayLine.prepare(spec);
}

void StereoResonator::Resonator::updateParameters(float frequency)
{
    this->gain = voice.getValue(params.gain);
    lastFrequency = nextFrequency;
    nextFrequency = frequency * std::pow(2.0f, voice.getValue(params.harmonicInSemitones, channel) / 12.0f);
    delayLengthInSamples = sampleRate / nextFrequency;
    delayLine.setDelay(delayLengthInSamples);
    decayCoefficient = std::pow(0.001f, 1.0f / (voice.getValue(params.decayTime, channel) * nextFrequency));
    apf.setDispersionAmount(voice.getValue(params.dispersion, channel));
    svf.setMode(0);
    svf.setCutoffFrequency<false>(5000.0f);
    svf.setQValue<false>(0.707f);
    svf.update();
}

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

void StereoResonator::updateParameters(float frequency, bool force)
{
    this->enabled = params.enabled->isOn();
    if(this->enabled || force)
    {
        resonators[0].updateParameters(frequency);
        resonators[1].updateParameters(frequency);

    } else
    {
        resonators[0].gain = 0;
        resonators[1].gain = 0;
    }
}
