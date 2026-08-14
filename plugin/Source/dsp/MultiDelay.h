#ifndef MULTIDELAY_H
#define MULTIDELAY_H

#include "DelayLine.h"

#include <juce_dsp/juce_dsp.h>
/**
* A delay effect with a continuous blend between stereo and ping-pong modes.
* Supports smooth delay time modulation with high-quality interpolation.
*/
class MultiDelay
{
public:
    explicit MultiDelay(float maxDelayInSeconds);

    void prepare(const juce::dsp::ProcessSpec& spec);
    void reset();

    void setPingPong(bool enabled);
    void setFeedback(int channel, float feedback);
    void setDelayTime(int channel, float time);
    void setMix(int channel, float mix);
    
    // Set smoothing time for delay time changes (in seconds)
    void setDelayTimeSmoothingTime(float smoothingTimeInSeconds);

    template <typename ProcessContext>
    void process(ProcessContext& context) noexcept
    {
        auto& inputBlock = context.getInputBlock();
        auto& outputBlock = context.getOutputBlock();
        jassert(inputBlock.getNumChannels() == 2);
        jassert(outputBlock.getNumChannels() == 2);

        const auto numSamples = inputBlock.getNumSamples();
        const auto* inputL = inputBlock.getChannelPointer(0);
        const auto* inputR = inputBlock.getChannelPointer(1);
        auto* outputL = outputBlock.getChannelPointer(0);
        auto* outputR = outputBlock.getChannelPointer(1);
        const bool stationary = !delayTimeSmoothL.isSmoothing() && !delayTimeSmoothR.isSmoothing();

        if (pingPongEnabled)
        {
            if (stationary)
                processSamples<true, true>(inputL, inputR, outputL, outputR, numSamples);
            else
                processSamples<true, false>(inputL, inputR, outputL, outputR, numSamples);
        }
        else
        {
            if (stationary)
                processSamples<false, true>(inputL, inputR, outputL, outputR, numSamples);
            else
                processSamples<false, false>(inputL, inputR, outputL, outputR, numSamples);
        }
    }

    float sampleRate = 44100.0f;
    float maxDelayInSeconds;
    bool pingPongEnabled;
    float feedbackL;
    float feedbackR;
    float timeL;
    float timeR;
    float mixL;
    float mixR;

private:
    template <bool PingPong, bool Stationary>
    void processSamples(const float* inputL, const float* inputR,
                        float* outputL, float* outputR, size_t numSamples) noexcept
    {
        const auto dryL = 1.0f - mixL;
        const auto dryR = 1.0f - mixR;

        if constexpr (Stationary)
        {
            delayLineL.setDelay(delayTimeSmoothL.getTargetValue() * sampleRate);
            delayLineR.setDelay(delayTimeSmoothR.getTargetValue() * sampleRate);
        }

        for (size_t i = 0; i < numSamples; ++i)
        {
            float leftDelay = 0.0f;
            float rightDelay = 0.0f;
            if constexpr (Stationary)
            {
                leftDelay = delayLineL.popSample(0);
                rightDelay = delayLineR.popSample(0);
            }
            else
            {
                leftDelay = delayLineL.popSample(0, delayTimeSmoothL.getNextValue() * sampleRate, true);
                rightDelay = delayLineR.popSample(0, delayTimeSmoothR.getNextValue() * sampleRate, true);
            }

            const auto leftIn = inputL[i];
            const auto rightIn = inputR[i];
            float leftDelayInput = 0.0f;
            float rightDelayInput = 0.0f;

            if constexpr (PingPong)
            {
                const auto monoInput = (leftIn + rightIn) * 0.5f;
                leftDelayInput = monoInput + rightDelay * feedbackL;
                rightDelayInput = leftDelay * feedbackR;
            }
            else
            {
                leftDelayInput = leftIn + leftDelay * feedbackL;
                rightDelayInput = rightIn + rightDelay * feedbackR;
            }

            delayLineL.pushSample(0, leftDelayInput);
            delayLineR.pushSample(0, rightDelayInput);
            outputL[i] = leftIn * dryL + leftDelay * mixL;
            outputR[i] = rightIn * dryR + rightDelay * mixR;
        }
    }

    // Preserve the original fifth-order interpolation used for smooth
    // effect-delay modulation. Resonator waveguides use the default third order.
    resonarium::DelayLine<float, resonarium::DelayInterpolation::lagrange5th> delayLineL;
    resonarium::DelayLine<float, resonarium::DelayInterpolation::lagrange5th> delayLineR;
    
    // Use juce::SmoothedValue to smooth delay time changes
    juce::SmoothedValue<float> delayTimeSmoothL;
    juce::SmoothedValue<float> delayTimeSmoothR;
    float smoothingTimeInSeconds = 0.05f; // Default 50ms smoothing
};

#endif //MULTIDELAY_H
