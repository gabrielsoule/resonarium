#include "MultiDelay.h"

#include <cmath>

MultiDelay::MultiDelay(float maxDelayInSeconds) :
    maxDelayInSeconds(maxDelayInSeconds),
    delayLineL(static_cast<int>(std::ceil(48000.0 * static_cast<double>(maxDelayInSeconds)))),
    delayLineR(static_cast<int>(std::ceil(48000.0 * static_cast<double>(maxDelayInSeconds))))
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
    const auto maximumDelayInSamples = static_cast<int>(
        std::ceil(spec.sampleRate * static_cast<double>(maxDelayInSeconds)));
    jassert(maximumDelayInSamples > 0);

    // Capacity depends on the host rate. Reconstructing here is safe because
    // prepare() is non-realtime and guarantees that every advertised delay time
    // remains valid at 44.1--96 kHz (and at other host rates).
    delayLineL = decltype(delayLineL)(maximumDelayInSamples);
    delayLineR = decltype(delayLineR)(maximumDelayInSamples);
    delayLineL.prepare(1);
    delayLineR.prepare(1);
    sampleRate = static_cast<float>(spec.sampleRate);
    
    // Update smoothers with correct sample rate
    delayTimeSmoothL.reset(sampleRate, smoothingTimeInSeconds);
    delayTimeSmoothR.reset(sampleRate, smoothingTimeInSeconds);
    delayTimeSmoothL.setCurrentAndTargetValue(timeL);
    delayTimeSmoothR.setCurrentAndTargetValue(timeR);
    
    reset();
}

void MultiDelay::reset()
{
    delayLineL.reset();
    delayLineR.reset();
    
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
    time = juce::jlimit(0.0f, maxDelayInSeconds, time);
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
