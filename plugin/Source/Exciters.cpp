#include "Exciters.h"
#include "ResonatorVoice.h"

void ImpulseExciter::prepare(const juce::dsp::ProcessSpec& spec, ResonatorVoice* voice)
{
    Exciter::prepare(spec, voice);
    filter.prepare(spec, voice);
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
    if (firstBlock)
    {
        for (int i = 0; i < impulseLength; i++)
        {
            scratchBlock.setSample(0, i, 1);
            DBG("Setting impulse sample to 1");
        }
        firstBlock = false;
    }

    juce::dsp::ProcessContextReplacing<float> context(scratchBlock);
    // filter.process(context);
    //TODO Add proper multi channel support here
    block.add(scratchBlock);
    scratchBlock.clear();
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
    impulseLength = voice->getValue(params.thickness);
    firstBlock = true;
}

void ImpulseExciter::noteStopped(bool avoidTailOff)
{

}

void ImpulseExciter::updateParameters()
{
    filter.updateParameters();
}

void ImpulseTrainExciter::updateParameters()
{

}

void NoiseExciter::nextSample()
{

}

void NoiseExciter::process(juce::dsp::AudioBlock<float> block)
{

}

void NoiseExciter::reset()
{

}

void NoiseExciter::noteStarted()
{

}

void NoiseExciter::noteStopped(bool avoidTailOff)
{

}

void NoiseExciter::updateParameters()
{

}

void SampleExciter::nextSample()
{

}

void SampleExciter::process(juce::dsp::AudioBlock<float> block)
{

}

void SampleExciter::reset()
{

}

void SampleExciter::noteStarted()
{

}

void SampleExciter::noteStopped(bool avoidTailOff)
{

}

void SampleExciter::updateParameters()
{

}

void AudioInputExciter::nextSample()
{

}

void AudioInputExciter::process(juce::dsp::AudioBlock<float> block)
{

}

void AudioInputExciter::reset()
{

}

void AudioInputExciter::noteStarted()
{

}

void AudioInputExciter::noteStopped(bool avoidTailOff)
{

}

void AudioInputExciter::updateParameters()
{

}
