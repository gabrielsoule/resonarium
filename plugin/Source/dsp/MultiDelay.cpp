#include "MultiDelay.h"

MultiDelay::MultiDelay(float maxDelayInSeconds) :
    maxDelayInSeconds(maxDelayInSeconds),
    delayLine(48000 * maxDelayInSeconds)
{

}

void MultiDelay::prepare(const juce::dsp::ProcessSpec& spec)
{
    delayLine.prepare(spec);
    sampleRate = spec.sampleRate;
}

void MultiDelay::reset()
{
    delayLine.reset();
}

void MultiDelay::setPingPongAmount(int channel, float newAmount)
{
    jassert(newAmount >= 0.0f && newAmount <= 1.0f);
    jassert(channel == 0 || channel == 1);
    if (channel == 0)
    {
        pingPongAmountL = newAmount;
    }
    else
    {
        pingPongAmountR = newAmount;
    }
}

void MultiDelay::setFeedback(int channel, float newFeedback)
{
    jassert(newFeedback >= 0.0f && newFeedback <= 1.0f);
    jassert(channel == 0 || channel == 1);
    if (channel == 0)
    {
        feedbackL = newFeedback;
    }
    else
    {
        feedbackR = newFeedback;
    }
}

void MultiDelay::setDelayTime(int channel, float time)
{
    jassert(channel == 0 || channel == 1);
    if (channel == 0)
    {
        timeL = time;
    }
    else
    {
        timeR = time;
    }
}

void MultiDelay::setMix(int channel, float newMix)
{
    jassert(newMix >= 0.0f && newMix <= 1.0f);
    jassert(channel == 0 || channel == 1);
    if (channel == 0)
    {
        mixL = newMix;
    }
    else
    {
        mixR = newMix;
    }
}
