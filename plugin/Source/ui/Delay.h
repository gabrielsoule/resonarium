#include <JuceHeader.h>

class Delay
{
public:
    Delay() : leftDelay(92000), rightDelay(92000)
    {

    }

    void setDelay(float timeInSeconds)
    {
        delayTime = timeInSeconds;
        auto delaySamples = static_cast<int>(timeInSeconds * sampleRate);
        leftDelay.setDelay(delaySamples);
        rightDelay.setDelay(delaySamples);
    }

    void setFeedback(float fb)
    {
        feedback = fb;
    }

    void setWetDry(float mix)
    {
        wetDry = mix;
    }

    void prepare(const juce::dsp::ProcessSpec& spec)
    {
        sampleRate = spec.sampleRate;
        leftDelay.prepare(spec);
        rightDelay.prepare(spec);
    }

    void processBlock(juce::AudioBuffer<float>& buffer)
    {
        auto numSamples = buffer.getNumSamples();
        auto* leftChannel = buffer.getWritePointer(0);
        auto* rightChannel = buffer.getWritePointer(1);

        for (int i = 0; i < numSamples; ++i)
        {
            float leftIn = leftChannel[i];
            float rightIn = rightChannel[i];

            float leftDelayedSample = leftDelay.popSample(0);
            float rightDelayedSample = rightDelay.popSample(0);

            leftDelay.pushSample(0, rightIn + rightDelayedSample * feedback);
            rightDelay.pushSample(0, leftIn + leftDelayedSample * feedback);

            float leftOut = leftIn * (1.0f - wetDry) + leftDelayedSample * wetDry;
            float rightOut = rightIn * (1.0f - wetDry) + rightDelayedSample * wetDry;

            leftChannel[i] = leftOut;
            rightChannel[i] = rightOut;
        }
    }

private:
    chowdsp::DelayLine<float, chowdsp::DelayLineInterpolationTypes::Linear> leftDelay;
    chowdsp::DelayLine<float, chowdsp::DelayLineInterpolationTypes::Linear> rightDelay;
    float delayTime = 0.0f;
    float feedback = 0.0f;
    float wetDry = 0.0f;
    double sampleRate = 44100.0;
};