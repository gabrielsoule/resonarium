#include "StereoMSEGWrapper.h"

void StereoMSEGWrapper::prepare(const juce::dsp::ProcessSpec& spec)
{
    left.setSampleRate(spec.sampleRate);
    right.setSampleRate(spec.sampleRate);
    left.reset();
    right.reset();
}

void StereoMSEGWrapper::reset()
{
    left.reset();
    right.reset();
}

void StereoMSEGWrapper::noteOn(float phase)
{
    left.noteOn(phase);
}

void StereoMSEGWrapper::process(int numSamples)
{
    left.process(numSamples);
    right.process(numSamples);
}

float StereoMSEGWrapper::getOutput(int channel)
{
    jassert(channel == 0 || channel == 1);
    return channel == 0 ? left.getOutput() : right.getOutput();
}

float StereoMSEGWrapper::getOutputUnclamped(int channel)
{
    jassertfalse;
    return -1;
}

float StereoMSEGWrapper::getCurrentPhase(int channel)
{
    jassert(channel == 0 || channel == 1);
    return channel == 0 ? left.getCurrentPhase() : right.getCurrentPhase();
}
