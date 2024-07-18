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
    jassert(block.getNumChannels() == 2);
    if(!params.enabled->isOn()) return;

    juce::dsp::AudioBlock<float> truncatedBlock = scratchBlock.getSubBlock(0, (size_t)block.getNumSamples());
    auto adjustedGain =level / static_cast<float>(thickness);
    const int impulsesThisBlock = juce::jmin<int>(truncatedBlock.getNumSamples(), impulsesRemaining);
    for (int i = 0; i < impulsesThisBlock; i++)
    {
        auto sample = (polarityFlag) ? adjustedGain : -adjustedGain;
        truncatedBlock.setSample(0, i, sample);
        truncatedBlock.setSample(1, i, sample);
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
    impulsesRemaining = thickness;
}

void ImpulseExciter::noteStopped(bool avoidTailOff)
{
}

void ImpulseExciter::updateParameters()
{
    if(!params.enabled->isOn()) return;
    thickness = voice.getValue(params.thickness);
    level = voice.getValue(params.level);
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
    jassert(block.getNumChannels() == 2);
    if(!params.enabled->isOn()) return;

    juce::dsp::AudioBlock<float> truncatedBlock = scratchBlock.getSubBlock(0, (size_t)block.getNumSamples());
    auto gainL = voice.getValue(params.level, 0);
    auto gainR = voice.getValue(params.level, 1);
    for (int i = 0; i < truncatedBlock.getNumSamples(); i++)
    {
        const float envSample = envelope.process();
        const float sampleL = noise.nextValue() * gainL * envSample;
        const float sampleR = noise.nextValue() * gainR * envSample;
        truncatedBlock.setSample(0, i, sampleL);
        truncatedBlock.setSample(1, i, sampleR);

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
    if(!params.enabled->isOn()) return;

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
    if(!params.enabled->isOn()) return;

    jassert(periodInSamples > 0);
    jassert(impulseLength > 0);
    jassert(samplesSinceLastImpulse >= 0);
    jassert(impulsesLeft >= 0);
    juce::dsp::AudioBlock<float> truncatedBlock = scratchBlock.getSubBlock(0, (size_t)block.getNumSamples());
    auto gain = voice.getValue(params.level);

    for (int i = 0; i < truncatedBlock.getNumSamples(); i++)
    {
        //all per-sample processing that happens regardless of mode goes here
        float envelopeSample = gain * envelope.process();
        if (mode == IMPULSE)
        {
            if (samplesSinceLastImpulse == 0)
            {
                impulsesLeft = impulseLength;
            }

            if (impulsesLeft > 0)
            {
                truncatedBlock.setSample(0, i, envelopeSample);
                truncatedBlock.setSample(1, i, envelopeSample);
                impulsesLeft--;
            }
            else
            {
                truncatedBlock.setSample(0, i, 0.0f);
                truncatedBlock.setSample(1, i, 0.0f);
            }

            samplesSinceLastImpulse--;
            if (samplesSinceLastImpulse < 0)
            {
                samplesSinceLastImpulse = periodInSamples - 1;
            }
        }
        else if (mode == STATIC)
        {
            float r = rng.nextFloat();
            if (r < staticProbability)
            {
                const float sample  = noise.nextValue() * envelopeSample;
                truncatedBlock.setSample(0, i, sample);
                truncatedBlock.setSample(1, i, sample);
            }
            else
            {
                truncatedBlock.setSample(0, i, 0.0f);
                truncatedBlock.setSample(1, i, 0.0f);
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

    filter.process(truncatedBlock);

    block.add(truncatedBlock);
}

void ImpulseTrainExciter::reset()
{
    noise.reset();
    envelope.reset();
    filter.reset();
    samplesSinceLastImpulse = 0;
    rng.setSeedRandomly();
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
    if(!params.enabled->isOn()) return;

    envelope.setAttack(voice.getValue(params.adsrParams.attack));
    envelope.setDecay(voice.getValue(params.adsrParams.decay));
    envelope.setSustainLevel(voice.getValue(params.adsrParams.sustain));
    envelope.setRelease(voice.getValue(params.adsrParams.release));
    filter.updateParameters();
    mode = static_cast<Mode>(voice.getValue(params.mode));
    character = voice.getValue(params.character);
    entropy = voice.getValue(params.entropy);
    periodInSamples = static_cast<int>(std::round(sampleRate / voice.getValue(params.rate)));

    //compute values for the different impulse train modes
    if(mode == IMPULSE)
    {
        impulseLength = static_cast<int>(std::round((voice.getValue(params.character) * 15.0f)));
        if (impulseLength <= 0) impulseLength = 1;

    } else if (mode == STATIC)
    {
        staticProbability = (character / 7.0f);
        staticProbability = staticProbability * staticProbability;
    }

    jassert(periodInSamples > 0);
    jassert(impulseLength > 0);
}
