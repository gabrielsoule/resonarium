#include "StereoMSEGWrapper.h"

StereoMSEGWrapper::StereoMSEGWrapper(MSEGParams params)
    : dataStore(params.msegData),
      audioData(std::make_shared<gin::MSEG::Data>()),
      left(*audioData),
      right(*audioData),
      params(std::move(params))
{
    syncDataSnapshot();
}

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
    syncDataSnapshot();
    left.noteOn(phase);
    right.noteOn(phase);
}

void StereoMSEGWrapper::process(int numSamples)
{
    syncDataSnapshot();
    left.process(numSamples);
    right.process(numSamples);
}

void StereoMSEGWrapper::syncDataSnapshot() noexcept
{
    if (dataStore == nullptr || dataStore->getPublishedVersion() == dataVersion)
        return;

    static_cast<void>(dataStore->copyLatestAudioData(*audioData, dataVersion));
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
