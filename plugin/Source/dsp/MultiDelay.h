#ifndef MULTIDELAY_H
#define MULTIDELAY_H

#include <chowdsp_dsp_utils/chowdsp_dsp_utils.h>
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
        // For debugging, forceWy ping-pong on
        // pingPongEnabled = true;
        
        auto& inputBlock = context.getInputBlock();
        auto& outputBlock = context.getOutputBlock();
        auto numChannels = inputBlock.getNumChannels();
        auto numSamples = inputBlock.getNumSamples();

        jassert(numChannels == 2);

        for (size_t i = 0; i < numSamples; ++i)
        {            
            // Update smoothed delay times
            const float smoothedTimeL = delayTimeSmoothL.getNextValue();
            const float smoothedTimeR = delayTimeSmoothR.getNextValue();
            
            // Get input samples
            const float leftIn = inputBlock.getSample(0, i);
            const float rightIn = inputBlock.getSample(1, i);

            // Read from delay line - use smooth modulation
            const float leftDelay = delayLine.popSample(0, smoothedTimeL * sampleRate, true);
            const float rightDelay = delayLine.popSample(1, smoothedTimeR * sampleRate, true);

            float leftDelayInput, rightDelayInput;
            
            if (pingPongEnabled)
            {
                // The simple ping-pong implementation:
                
                // 1. Convert input to mono
                const float monoInput = (leftIn + rightIn) * 0.5f;
                
                // 2. Send mono input to the LEFT channel only
                // (We could choose either channel, but let's use left as the starting point)
                
                // 3. Cross-feedback: swap the channels in the feedback path
                const float leftFeedback = rightDelay * feedbackL;  // Left gets feedback from RIGHT
                const float rightFeedback = leftDelay * feedbackR;  // Right gets feedback from LEFT
                
                // 4. Apply input to left channel only, both channels get cross-feedback
                leftDelayInput = monoInput + leftFeedback;   // Left gets mono input + right's feedback
                rightDelayInput = rightFeedback;             // Right only gets feedback from left
            }
            else
            {
                // Normal stereo delay (no ping-pong)
                const float leftFeedback = leftDelay * feedbackL;
                const float rightFeedback = rightDelay * feedbackR;
                
                leftDelayInput = leftIn + leftFeedback;
                rightDelayInput = rightIn + rightFeedback;
            }
            
            // Push new samples to delay line
            delayLine.pushSample(0, leftDelayInput);
            delayLine.pushSample(1, rightDelayInput);

            // Use the original delayed signals for wet output
            const float leftWet = leftDelay;
            const float rightWet = rightDelay;
            
            // Mix dry and wet signals
            const float leftOut = leftIn * (1.0f - mixL) + leftWet * mixL;
            const float rightOut = rightIn * (1.0f - mixR) + rightWet * mixR;

            // Write output
            outputBlock.setSample(0, i, leftOut);
            outputBlock.setSample(1, i, rightOut);
        }
    }

    float sampleRate;
    float maxDelayInSeconds;
    bool pingPongEnabled;
    float feedbackL;
    float feedbackR;
    float timeL;
    float timeR;
    float mixL;
    float mixR;

private:
    // Use higher quality interpolation (5th order Lagrange) for better results during modulation
    chowdsp::DelayLine<float, chowdsp::DelayLineInterpolationTypes::Lagrange5th> delayLine;
    
    // Use juce::SmoothedValue to smooth delay time changes
    juce::SmoothedValue<float> delayTimeSmoothL;
    juce::SmoothedValue<float> delayTimeSmoothR;
    float smoothingTimeInSeconds = 0.05f; // Default 50ms smoothing
};

#endif //MULTIDELAY_H