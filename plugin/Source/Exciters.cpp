#include "Exciters.h"
#include "ResonatorVoice.h"

void ImpulseExciter::prepare(const juce::dsp::ProcessSpec& spec)
{
    filter.prepare(spec);
    filter.normalize = false;
    scratchBuffer = juce::AudioBuffer<float>(spec.numChannels, spec.maximumBlockSize);
    scratchBuffer.clear();
    scratchBlock = juce::dsp::AudioBlock<float>(scratchBuffer);
    reset();
}

void ImpulseExciter::nextSample()
{

}

void ImpulseExciter::process(juce::dsp::AudioBlock<float>& block)
{
    juce::dsp::AudioBlock<float> truncatedBlock = scratchBlock.getSubBlock(0, (size_t)block.getNumSamples());
    auto gain = voice.getValue(params.gain);
    const int impulsesThisBlock = juce::jmin<int>(truncatedBlock.getNumSamples(), impulsesRemaining);
    for (int i = 0; i < impulsesThisBlock; i++)
    {
        truncatedBlock.setSample(0, i, gain * 2);
    }

    impulsesRemaining -= impulsesThisBlock;

    filter.process(truncatedBlock);
    //TODO Add proper multi channel support here
    block.add(truncatedBlock);
    scratchBlock.clear();
}

void ImpulseExciter::reset()
{
    filter.reset();
    scratchBlock.clear();
    impulsesRemaining = voice.getValue(params.thickness);
}

void ImpulseExciter::noteStarted()
{
    impulsesRemaining = voice.getValue(params.thickness);
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

void NoiseExciter::process(juce::dsp::AudioBlock<float>& block)
{
    juce::dsp::AudioBlock<float> truncatedBlock = scratchBlock.getSubBlock(0, (size_t)block.getNumSamples());
    auto gain = voice.getValue(params.gain);
    for (int i = 0; i < truncatedBlock.getNumSamples(); i++)
    {
        truncatedBlock.setSample(0, i, noise.nextValue() * gain * envelope.process());
    }

    filter.process(truncatedBlock);
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

void ImpulseTrainExciter::prepare(const juce::dsp::ProcessSpec& spec)
{
    filter.prepare(spec);
    filter.normalize = false;
    scratchBuffer = juce::AudioBuffer<float>(spec.numChannels, spec.maximumBlockSize);
    scratchBuffer.clear();
    scratchBlock = juce::dsp::AudioBlock<float>(scratchBuffer);
    sampleRate = spec.sampleRate;
    reset();
}

void ImpulseTrainExciter::nextSample()
{
}

void ImpulseTrainExciter::process(juce::dsp::AudioBlock<float>& block)
{
    jassert(periodInSamples > 0);
    jassert(impulseLength > 0);
    jassert(samplesSinceLastImpulse >= 0);
    jassert(impulsesLeft >= 0);
    juce::dsp::AudioBlock<float> truncatedBlock = scratchBlock.getSubBlock(0, (size_t)block.getNumSamples());
    auto gain = voice.getValue(params.gain);
    for (int i = 0; i < truncatedBlock.getNumSamples(); i++)
    {
        if (mode == IMPULSE)
        {
            if (samplesSinceLastImpulse == 0)
            {
                impulsesLeft = impulseLength;
            }

            float sample = gain * envelope.process();

            if (impulsesLeft > 0)
            {
                truncatedBlock.setSample(0, i, sample);
                impulsesLeft--;
            }
            else
            {
                truncatedBlock.setSample(0, i, 0.0f);
            }

            samplesSinceLastImpulse--;
            if (samplesSinceLastImpulse < 0)
            {
                samplesSinceLastImpulse = periodInSamples - 1;
            }
        }
        else if (mode == PULSE)
        {
            jassertfalse;
        }
        else if (mode == NOISE_BURST)
        {
            jassertfalse;
        }
        else
        {
            jassertfalse;
        }
    }

    block.add(truncatedBlock);
}

void ImpulseTrainExciter::reset()
{
    noise.reset();
    envelope.reset();
    filter.reset();
    samplesSinceLastImpulse = 0;
    impulsesLeft = impulseLength;
}

void ImpulseTrainExciter::noteStarted()
{
    envelope.noteOn();
}

void ImpulseTrainExciter::noteStopped(bool avoidTailOff)
{
    envelope.noteOff();
}

void ImpulseTrainExciter::updateParameters()
{
    envelope.setAttack(voice.getValue(params.adsrParams.attack));
    envelope.setDecay(voice.getValue(params.adsrParams.decay));
    envelope.setSustainLevel(voice.getValue(params.adsrParams.sustain));
    envelope.setRelease(voice.getValue(params.adsrParams.release));
    filter.updateParameters();
    mode = static_cast<Mode>(voice.getValue(params.mode));
    periodInSamples = static_cast<int>(std::round(sampleRate / voice.getValue(params.speed)));
    impulseLength = static_cast<int>(std::round((voice.getValue(params.character) * 15.0f)));
    if (impulseLength <= 0) impulseLength = 1;
    jassert(periodInSamples > 0);
    jassert(impulseLength > 0);

}
