#include "Exciters.h"

#include "PluginProcessor.h"
#include "ResonatorVoice.h"

void ImpulseExciter::prepare(const juce::dsp::ProcessSpec& spec)
{
    Exciter::prepare(spec);
    filter.prepare(spec);
    filter.normalize = false;
    scratchBuffer = juce::AudioBuffer<float>(spec.numChannels, spec.maximumBlockSize);
    scratchBlock = juce::dsp::AudioBlock<float>(scratchBuffer);
    reset();
}

void ImpulseExciter::process(juce::dsp::AudioBlock<float>& exciterBlock, juce::dsp::AudioBlock<float>& outputBlock)
{
    jassert(exciterBlock.getNumChannels() == 2);
    if (!params.enabled->isOn()) return;

    juce::dsp::AudioBlock<float> truncatedBlock = scratchBlock.getSubBlock(0, (size_t)exciterBlock.getNumSamples());
    truncatedBlock.clear();
    auto adjustedGain = level / static_cast<float>(thickness);
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
    exciterBlock.add(truncatedBlock);
}

void ImpulseExciter::reset()
{
    filter.reset();
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
    if (!params.enabled->isOn()) return;
    thickness = static_cast<int>(voice.getValue(params.thickness));
    if (thickness < 1) thickness = 1;
    level = voice.getValue(params.level);
    filter.updateParameters();
}

void NoiseExciter::prepare(const juce::dsp::ProcessSpec& spec)
{
    Exciter::prepare(spec);
    filter.prepare(spec);
    scratchBuffer = juce::AudioBuffer<float>(spec.numChannels, spec.maximumBlockSize);
    scratchBuffer.clear();
    scratchBlock = juce::dsp::AudioBlock<float>(scratchBuffer);
    reset();
}

void NoiseExciter::process(juce::dsp::AudioBlock<float>& exciterBlock, juce::dsp::AudioBlock<float>& outputBlock)
{
    jassert(exciterBlock.getNumChannels() == 2);
    if (!params.enabled->isOn()) return;

    juce::dsp::AudioBlock<float> truncatedBlock = scratchBlock.getSubBlock(0, (size_t)exciterBlock.getNumSamples());
    auto gainL = voice.getValue(params.level, 0);
    auto gainR = voice.getValue(params.level, 1);
    for (int i = 0; i < truncatedBlock.getNumSamples(); i++)
    {
        const float envSample = envelope.process();
        const float noiseSample = noise.nextValue() * 0.1;
        const float sampleL = noiseSample * gainL * envSample;
        const float sampleR = noiseSample * gainR * envSample;
        truncatedBlock.setSample(0, i, sampleL);
        truncatedBlock.setSample(1, i, sampleR);
    }
    filter.process(truncatedBlock);
    exciterBlock.add(truncatedBlock);
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
    if (!params.enabled->isOn()) return;

    envelope.setAttack(voice.getValue(params.adsrParams.attack));
    envelope.setDecay(voice.getValue(params.adsrParams.decay));
    envelope.setSustainLevel(voice.getValue(params.adsrParams.sustain));
    envelope.setRelease(voice.getValue(params.adsrParams.release));
    filter.updateParameters();
}

void SequenceExciter::prepare(const juce::dsp::ProcessSpec& spec)
{
    Exciter::prepare(spec);
    filter.prepare(spec);
    filter.normalize = false;
    scratchBuffer = juce::AudioBuffer<float>(spec.numChannels, spec.maximumBlockSize);
    scratchBuffer.clear();
    scratchBlock = juce::dsp::AudioBlock<float>(scratchBuffer);
    sampleRate = spec.sampleRate;
    reset();
}

void SequenceExciter::process(juce::dsp::AudioBlock<float>& exciterBlock, juce::dsp::AudioBlock<float>& outputBlock)
{
    if (!params.enabled->isOn()) return;

    juce::dsp::AudioBlock<float> truncatedBlock = scratchBlock.getSubBlock(0, (size_t)exciterBlock.getNumSamples());
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
                const float sample = noise.nextValue() * envelopeSample;
                truncatedBlock.setSample(0, i, sample);
                truncatedBlock.setSample(1, i, sample);
            }
            else
            {
                truncatedBlock.setSample(0, i, 0.0f);
                truncatedBlock.setSample(1, i, 0.0f);
            }
        }
        else if (mode == TRIANGULAR)
        {
            const float value = currentTriValue +
                               (targetTriValue - currentTriValue) * triPhase;

            const float sample = (value * 2 - 1.0f) * envelopeSample;

            truncatedBlock.setSample(0, i, sample);
            truncatedBlock.setSample(1, i, sample);

            triPhase += triPhaseInc;

            if (triPhase >= 1.0f)
            {
                currentTriValue = targetTriValue;
                targetTriValue = rng.nextFloat() * (triAmpMax - triAmpMin) + triAmpMin;
                triPhase -= 1.0f;
                DBG("Setting target to" + juce::String(targetTriValue));
            }
        } else
        {
            jassertfalse;
        }

    }

    filter.process(truncatedBlock);

    exciterBlock.add(truncatedBlock);
}

void SequenceExciter::reset()
{
    noise.reset();
    envelope.reset();
    filter.reset();
    samplesSinceLastImpulse = 0;
    rng.setSeedRandomly();
    impulsesLeft = impulseLength;

    currentTriValue = 0.0f;
    targetTriValue = 0.0f;
    triPhase = 0.0f;
    triPhaseInc = 0.0f;
    triAmpMin = 0.0f;
    triAmpMax = 1.0f;
    prevPeriodInSamples = -1;
}

void SequenceExciter::noteStarted()
{
    periodInSamples = static_cast<int>(std::round(sampleRate / voice.getValue(params.rate)));
    envelope.noteOn();
}

void SequenceExciter::noteStopped(bool avoidTailOff)
{
    envelope.noteOff();
}

void SequenceExciter::updateParameters()
{
    if (!params.enabled->isOn()) return;

    envelope.setAttack(voice.getValue(params.adsrParams.attack));
    envelope.setDecay(voice.getValue(params.adsrParams.decay));
    envelope.setSustainLevel(voice.getValue(params.adsrParams.sustain));
    envelope.setRelease(voice.getValue(params.adsrParams.release));
    filter.updateParameters();
    mode = static_cast<Mode>(voice.getValue(params.mode));
    character = voice.getValue(params.character);
    entropy = voice.getValue(params.entropy);
    const float newPeriod = static_cast<int>(std::round(sampleRate / voice.getValue(params.rate)));

    //compute values for the different impulse train modes
    if (mode == IMPULSE)
    {
        impulseLength = static_cast<int>(std::round((voice.getValue(params.character) * 15.0f)));
        if (impulseLength <= 0) impulseLength = 1;
    }
    else if (mode == STATIC)
    {
        staticProbability = (character / 7.0f);
        staticProbability = staticProbability * staticProbability;
    }
    else if (mode == TRIANGULAR)
    {
        // Handle period changes and phase continuity
        if (newPeriod != periodInSamples)
        {
            const float periodRatio = static_cast<float>(periodInSamples) / newPeriod;
            triPhase *= periodRatio;

            // Handle phase overflow from period decreases
            while (triPhase >= 1.0f)
            {
                triPhase -= 1.0f;
                currentTriValue = targetTriValue;
                targetTriValue = rng.nextFloat() * (triAmpMax - triAmpMin) + triAmpMin;
            }
        }

        triPhaseInc = 1.0f / newPeriod;
        triAmpMin = 0.0f; // Add these parameters
        triAmpMax = 1.0f;  // to your parameter struct
    }

    periodInSamples = newPeriod;
    jassert(periodInSamples > 0);
}

void SampleExciter::prepare(const juce::dsp::ProcessSpec& spec)
{
    Exciter::prepare(spec);
    filter.prepare(spec);
    scratchBuffer = juce::AudioBuffer<float>(spec.numChannels, spec.maximumBlockSize);
    scratchBuffer.clear();
    scratchBlock = juce::dsp::AudioBlock<float>(scratchBuffer);
}

void SampleExciter::process(juce::dsp::AudioBlock<float>& exciterBlock, juce::dsp::AudioBlock<float>& outputBlock)
{
    if (!params.enabled->isOn() || !state.sampler.isLoaded() || !isPlaying)
        return;

    const int numSamples = exciterBlock.getNumSamples();
    const int loopLength = endSample - startSample;
    int remainingSamples = numSamples;
    int blockOffset = 0;

    while (remainingSamples > 0)
    {
        // Check if we've hit the end point
        if (currentSample >= endSample)
        {
            if (params.loop->isOn())
            {
                // Wrap back to start point
                currentSample = startSample + (currentSample - endSample) % loopLength;
            }
            else
            {
                isPlaying = false;
                break;
            }
        }

        // Calculate available samples until end point
        const int samplesAvailable = endSample - currentSample;
        const int samplesToProcess = std::min(remainingSamples, samplesAvailable);

        // Get the raw sample data
        auto sampleBlock = state.sampler.getSubBlock(currentSample, samplesToProcess);

        // Copy to our scratch buffer
        juce::dsp::AudioBlock<float> tempBlock = scratchBlock.getSubBlock(0, samplesToProcess);
        tempBlock.copyFrom(sampleBlock);

        // Filter the scratch buffer
        filter.process(tempBlock);
        envelope.processMultiplying(scratchBuffer, 0, samplesToProcess);

        // Now get the output blocks
        auto exciterSubBlock = exciterBlock.getSubBlock(blockOffset, samplesToProcess);
        auto outBlock = outputBlock.getSubBlock(blockOffset, samplesToProcess);

        // Add the filtered signal to both outputs with appropriate scaling
        exciterSubBlock.addProductOf(tempBlock, level * mixL);
        outBlock.addProductOf(tempBlock, level * (1.0f - mixL));

        currentSample += samplesToProcess;
        remainingSamples -= samplesToProcess;
        blockOffset += samplesToProcess;

        if (envelope.getState() == gin::AnalogADSR::State::idle)
        {
            isPlaying = false;
        }
    }
}

void SampleExciter::reset()
{
    currentSample = 0;
    envelope.reset();
    filter.reset();
    scratchBlock.clear();
}

void SampleExciter::noteStarted()
{
    isPlaying = true;
    envelope.noteOn();
    startSample = static_cast<int>(state.sampler.getNumSamples() * voice.getValue(params.start));
    endSample = static_cast<int>(state.sampler.getNumSamples() * voice.getValue(params.end));
}

void SampleExciter::noteStopped(bool avoidTailOff)
{
    envelope.noteOff();
}

void SampleExciter::updateParameters()
{
    mixL = voice.getValue(params.mix, 0);
    mixR = voice.getValue(params.mix, 1);
    level = voice.getValue(params.level, 0);

    filter.updateParameters();
    envelope.setAttack(voice.getValue(params.adsrParams.attack));
    envelope.setDecay(voice.getValue(params.adsrParams.decay));
    envelope.setSustainLevel(voice.getValue(params.adsrParams.sustain));
    envelope.setRelease(voice.getValue(params.adsrParams.release));
}

void ExternalInputExciter::prepare(const juce::dsp::ProcessSpec& spec)
{
    Exciter::prepare(spec);
    this->extInBuffer = juce::AudioBuffer<float>(spec.numChannels, spec.maximumBlockSize);
    extInBuffer.clear();
    this->extInBlock = juce::dsp::AudioBlock<float>(extInBuffer);
    reset();
}

void ExternalInputExciter::process(juce::dsp::AudioBlock<float>& exciterBlock,
                                   juce::dsp::AudioBlock<float>& outputBlock)
{
    if (!params.enabled->isOn()) return;

    const auto& v = dynamic_cast<ResonatorVoice&>(this->voice);
    const float mix = std::sin(voice.getValue(params.mix) * juce::MathConstants<float>::halfPi);
    const float gainL = voice.getValue(params.gain, 0) * mix;
    const float gainR = voice.getValue(params.gain, 1) * mix;
    extInBuffer.copyFrom(0, v.currentBlockStartSample, state.extInputBuffer.getReadPointer(0), v.currentBlockNumSamples,
                         gainL);
    extInBuffer.copyFrom(1, v.currentBlockStartSample, state.extInputBuffer.getReadPointer(1), v.currentBlockNumSamples,
                         gainR);
    juce::dsp::AudioBlock<float> extInBlock = juce::dsp::AudioBlock<float>(extInBuffer, v.currentBlockStartSample);
    // filter.process(extInBlock);
    envelope.processMultiplying(extInBuffer, v.currentBlockStartSample, v.currentBlockNumSamples);
    exciterBlock.add(extInBlock);
}

void ExternalInputExciter::reset()
{
    filter.reset();
    envelope.reset();
}

void ExternalInputExciter::noteStarted()
{
    envelope.noteOn();
}

void ExternalInputExciter::noteStopped(bool avoidTailOff)
{
    envelope.noteOff();
}

void ExternalInputExciter::updateParameters()
{
    if (!params.enabled->isOn()) return;

    envelope.setAttack(voice.getValue(params.adsrParams.attack));
    envelope.setDecay(voice.getValue(params.adsrParams.decay));
    envelope.setSustainLevel(voice.getValue(params.adsrParams.sustain));
    envelope.setRelease(voice.getValue(params.adsrParams.release));
}
