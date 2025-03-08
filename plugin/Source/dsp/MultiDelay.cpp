#include "MultiDelay.h"

MultiDelay::MultiDelay(float maxDelayInSeconds) :
    maxDelayInSeconds(maxDelayInSeconds),
    delayLine(48000 * maxDelayInSeconds)
{
    // Initialize default values
    pingPongEnabled = false;
    feedbackL = feedbackR = 0.5f;
    timeL = timeR = 0.5f;
    mixL = mixR = 0.5f;
    
    // Initialize smoothed values
    delayTimeSmoothL.reset(44100, smoothingTimeInSeconds);
    delayTimeSmoothR.reset(44100, smoothingTimeInSeconds);
    delayTimeSmoothL.setCurrentAndTargetValue(timeL);
    delayTimeSmoothR.setCurrentAndTargetValue(timeR);
}

void MultiDelay::prepare(const juce::dsp::ProcessSpec& spec)
{
    delayLine.prepare(spec);
    sampleRate = spec.sampleRate;
    
    // Update smoothers with correct sample rate
    delayTimeSmoothL.reset(sampleRate, smoothingTimeInSeconds);
    delayTimeSmoothR.reset(sampleRate, smoothingTimeInSeconds);
    delayTimeSmoothL.setCurrentAndTargetValue(timeL);
    delayTimeSmoothR.setCurrentAndTargetValue(timeR);
    
    reset();
}

void MultiDelay::reset()
{
    delayLine.reset();
    
    // Reset smoothers to current delay times
    delayTimeSmoothL.setCurrentAndTargetValue(timeL);
    delayTimeSmoothR.setCurrentAndTargetValue(timeR);
}

void MultiDelay::setPingPong(bool enabled)
{
    pingPongEnabled = enabled;
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
        // Set the target value for smooth transitions
        timeL = time;
        delayTimeSmoothL.setTargetValue(time);
    }
    else
    {
        // Set the target value for smooth transitions
        timeR = time;
        delayTimeSmoothR.setTargetValue(time);
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

void MultiDelay::setDelayTimeSmoothingTime(float newSmoothingTimeInSeconds)
{
    smoothingTimeInSeconds = newSmoothingTimeInSeconds;
    
    // Update the smoothing time for both channels
    delayTimeSmoothL.reset(sampleRate, smoothingTimeInSeconds);
    delayTimeSmoothR.reset(sampleRate, smoothingTimeInSeconds);
}
