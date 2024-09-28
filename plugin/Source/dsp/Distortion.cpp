#include "Distortion.h"

Distortion::Distortion(ResonariumProcessor& p, DistortionParams params)
    : proc(p), params(params), distortionMode(SOFT_CLIP), drive(1.0f),
      lastDownSampleValue(0.0f), downSampleCounter(0.0f)
{
}

void Distortion::reset()
{
    lastDownSampleValue = 0.0f;
    downSampleCounter = 0.0f;
    filter.reset();
}

void Distortion::process(juce::dsp::ProcessContextReplacing<float> context)
{
    auto& inputBlock = context.getInputBlock();
    auto& outputBlock = context.getOutputBlock();
    auto numSamples = inputBlock.getNumSamples();
    auto numChannels = inputBlock.getNumChannels();

    jassert(inputBlock.getNumChannels() == 2);
    jassert(outputBlock.getNumChannels() == 2);

    if (filterMode == pre)
    {
        filter.process(context);
    }

    for (size_t channel = 0; channel < numChannels; ++channel)
    {
        auto* input = inputBlock.getChannelPointer(channel);
        auto* output = outputBlock.getChannelPointer(channel);
        float scaledDrive = std::pow(10.0f, drive[channel] / 20.0f);;
        for (size_t sample = 0; sample < numSamples; ++sample)
        {
            float inSample = input[sample];
            float outSample = inSample;

            switch (distortionMode)
            {
            case SOFT_CLIP:
                {
                    float scaledValue = inSample * scaledDrive;
                    outSample = std::tanh(scaledValue);
                    break;
                }
            case HARD_CLIP:
                {
                    float scaledValue = inSample * scaledDrive;
                    outSample = juce::jlimit(-1.0f, 1.0f, scaledValue);
                    break;
                }
            case LINEAR_FOLD:
                {
                    float adjust = inSample * scaledDrive * 0.25f + 0.75f;
                    float range = std::fmod(adjust, 1.0f);
                    outSample = std::abs(range * -4.0f + 2.0f) - 1.0f;
                    break;
                }
            case SIN_FOLD:
                {
                    float adjust = inSample * scaledDrive * -0.25f + 0.5f;
                    float range = std::fmod(adjust, 1.0f);
                    outSample = std::sin(range * 2.0f * juce::MathConstants<float>::pi);
                    break;
                }
            case BIT_CRUSH:
                {
                    int levels = juce::jlimit(1, 32, static_cast<int>(drive[channel]));
                    float step = 2.0f / levels; // Assuming input signal is in [-1, 1]
                    outSample = std::round(inSample / step) * step;
                    break;
                }
            case DOWN_SAMPLE:
                {
                    float period = juce::jmax(1.0f, drive[channel]);
                    downSampleCounter += 1.0f;
                    if (downSampleCounter >= period)
                    {
                        downSampleCounter -= period;
                        lastDownSampleValue = inSample;
                    }
                    outSample = lastDownSampleValue;
                    break;
                }
            default:
                {
                    // No processing
                    jassertfalse;
                    break;
                }
            }

            output[sample] = outSample;
        }
    }

    if (filterMode == post)
    {
        filter.process(context);
    }
}
