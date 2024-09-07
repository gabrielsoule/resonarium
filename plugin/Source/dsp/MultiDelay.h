#ifndef MULTIDELAY_H
#define MULTIDELAY_H

#include <JuceHeader.h>

/**
* A delay effect with a continuous blend between stereo and ping-pong modes.
*/
class MultiDelay
{
public:
    MultiDelay(float maxDelayInSeconds);

    void prepare(const juce::dsp::ProcessSpec& spec);
    void reset();

    void setPingPongAmount(int channel, float newAmount);
    void setFeedback(int channel, float feedback);
    void setDelayTime(int channel, float time);
    void setMix(int channel, float mix);

    template <typename ProcessContext>
    void process(ProcessContext& context) noexcept
    {
        auto& inputBlock = context.getInputBlock();
        auto& outputBlock = context.getOutputBlock();
        auto numChannels = inputBlock.getNumChannels();
        auto numSamples = inputBlock.getNumSamples();

        jassert(numChannels == 2);

        for (size_t i = 0; i < numSamples; ++i)
        {
            // Get input samples
            const float leftIn = inputBlock.getSample(0, i);
            const float rightIn = inputBlock.getSample(1, i);

            // Read from delay line
            const float leftDelay = delayLine.popSample(0, timeL * sampleRate, true);
            const float rightDelay = delayLine.popSample(1, timeR * sampleRate, true);

            // Calculate delay inputs with gradual ping-pong
            const float leftFeedback = juce::jmap(pingPongAmountL, leftDelay, rightDelay);
            const float rightFeedback = juce::jmap(pingPongAmountR, rightDelay, leftDelay);

            const float leftDelayInput = leftIn + leftFeedback * feedbackL;
            const float rightDelayInput = rightIn + rightFeedback * feedbackR;

            // Push new samples to delay line
            delayLine.pushSample(0, leftDelayInput);
            delayLine.pushSample(1, rightDelayInput);

            // Mix dry and wet signals
            const float leftOut = leftIn * (1.0f - mixL) + leftDelay * mixL;
            const float rightOut = rightIn * (1.0f - mixR) + rightDelay * mixR;

            // Write output
            outputBlock.setSample(0, i, leftOut);
            outputBlock.setSample(1, i, rightOut);
        }
    }

    float sampleRate;
    float maxDelayInSeconds;
    float pingPongAmountL;
    float pingPongAmountR;
    float feedbackL;
    float feedbackR;
    float timeL;
    float timeR;
    float mixL;
    float mixR;

private:
    chowdsp::DelayLine<float, chowdsp::DelayLineInterpolationTypes::Lagrange3rd> delayLine;
};

#endif //MULTIDELAY_H