#include "Exciters.h"
#include "ResonatorVoice.h"

void ImpulseExciter::prepare(const juce::dsp::ProcessSpec& spec)
{
    filter.prepare(spec);
    scratchBuffer = juce::AudioBuffer<float>(spec.numChannels, spec.maximumBlockSize);
    scratchBuffer.clear();
    scratchBlock = juce::dsp::AudioBlock<float>(scratchBuffer);
    reset();
}

void ImpulseExciter::nextSample()
{

}

void ImpulseExciter::process(juce::dsp::AudioBlock<float> block)
{
    juce::dsp::AudioBlock<float> truncatedBlock = scratchBlock.getSubBlock(0,  (size_t) block.getNumSamples());
    auto gain = voice.getValue(params.gain);
    if (firstBlock)
    {
        for (int i = 0; i < impulseLength; i++)
        {
            truncatedBlock.setSample(0, i, gain * 5);
        }
    }

    filter.process(juce::dsp::ProcessContextReplacing<float>(truncatedBlock));
    //TODO Add proper multi channel support here
    block.add(truncatedBlock);
    scratchBlock.clear();
    firstBlock = false;
}

void ImpulseExciter::reset()
{
    impulseCounter = 0;
    firstBlock = true;
    filter.reset();
    scratchBlock.clear();
}

void ImpulseExciter::noteStarted()
{
    impulseLength = voice.getValue(params.thickness);
    firstBlock = true;
}

void ImpulseExciter::noteStopped(bool avoidTailOff)
{

}

void ImpulseExciter::updateParameters()
{
    filter.updateParameters();
}

void NoiseExciter::prepare(const juce::dsp::ProcessSpec& spec)
{
    filter.prepare(spec);
    scratchBuffer = juce::AudioBuffer<float>(spec.numChannels, spec.maximumBlockSize);
    scratchBuffer.clear();
    scratchBlock = juce::dsp::AudioBlock<float>(scratchBuffer);
    reset();
}


void NoiseExciter::nextSample()
{

}

void NoiseExciter::process(juce::dsp::AudioBlock<float> block)
{
    juce::dsp::AudioBlock<float> truncatedBlock = scratchBlock.getSubBlock(0,  (size_t) block.getNumSamples());
    auto gain = voice.getValue(params.gain);
    for(int i = 0; i < truncatedBlock.getNumSamples(); i++)
    {
        truncatedBlock.setSample(0, i, noise.nextValue() * gain * envelope.process());
    }

    filter.process(juce::dsp::ProcessContextReplacing<float>(truncatedBlock));
    block.add(truncatedBlock);
}

void NoiseExciter::reset()
{
    noise.reset();
    envelope.reset();
    filter.reset();
}

void NoiseExciter::noteStarted()
{
    envelope.noteOn();
}

void NoiseExciter::noteStopped(bool avoidTailOff)
{
    envelope.noteOff();
}

void NoiseExciter::updateParameters()
{
    envelope.setAttack(voice.getValue(params.adsrParams.attack));
    envelope.setDecay(voice.getValue(params.adsrParams.decay));
    envelope.setSustainLevel(voice.getValue(params.adsrParams.sustain));
    envelope.setRelease(voice.getValue(params.adsrParams.release));
    filter.updateParameters();
}