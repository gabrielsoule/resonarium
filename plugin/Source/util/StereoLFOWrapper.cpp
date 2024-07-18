#include "StereoLFOWrapper.h"

void StereoLFOWrapper::prepare(const juce::dsp::ProcessSpec& spec)
{
    left.setSampleRate(spec.sampleRate);
    right.setSampleRate(spec.sampleRate);
}

void StereoLFOWrapper::reset()
{
    left.reset();
    right.reset();
}

void StereoLFOWrapper::noteOn(float phase = -1)
{
    left.noteOn(phase);
    right.noteOn(phase);
}

void StereoLFOWrapper::process(int numSamples)
{
    left.process(numSamples);
    right.process(numSamples);
}

float StereoLFOWrapper::getOutput(int channel)
{
    jassert(channel == 0 || channel == 1);
    return channel == 0 ? left.getOutput() : right.getOutput();
}

float StereoLFOWrapper::getOutputUnclamped(int channel)
{
    jassert(channel == 0 || channel == 1);
    return channel == 0 ? left.getOutputUnclamped() : right.getOutputUnclamped();
}

float StereoLFOWrapper::getCurrentPhase(int channel)
{
    jassert(channel == 0 || channel == 1);
    return channel == 0 ? left.getCurrentPhase() : right.getCurrentPhase();
}
