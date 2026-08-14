#include "Distortion.h"

Distortion::Distortion(DistortionParams params)
    : params(params), distortionMode(SOFT_CLIP)
{
    for (int channel = 0; channel < 2; ++channel)
    {
        scaledDrive[channel] = std::pow(10.0f, drive[channel] / 20.0f);
        scaledOutputGain[channel] = std::pow(10.0f, outputGain[channel] / 20.0f);
    }
}

void Distortion::prepare(const juce::dsp::ProcessSpec& spec)
{
    filter.prepare(spec);
    reset();
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
        const auto channelDrive = scaledDrive[channel];
        const auto channelOutput = scaledOutputGain[channel];

        switch (distortionMode)
        {
        case SOFT_CLIP:
            for (size_t sample = 0; sample < numSamples; ++sample)
                output[sample] = std::tanh(input[sample] * channelDrive) * channelOutput;
            break;
        case HARD_CLIP:
            for (size_t sample = 0; sample < numSamples; ++sample)
                output[sample] = juce::jlimit(-1.0f, 1.0f, input[sample] * channelDrive) * channelOutput;
            break;
        case LINEAR_FOLD:
            for (size_t sample = 0; sample < numSamples; ++sample)
            {
                const auto adjust = input[sample] * channelDrive * 0.25f + 0.75f;
                const auto range = std::fmod(adjust, 1.0f);
                output[sample] = (std::abs(range * -4.0f + 2.0f) - 1.0f) * channelOutput;
            }
            break;
        case SIN_FOLD:
            for (size_t sample = 0; sample < numSamples; ++sample)
            {
                const auto adjust = input[sample] * channelDrive * -0.25f + 0.5f;
                const auto range = std::fmod(adjust, 1.0f);
                output[sample] = std::sin(range * 2.0f * juce::MathConstants<float>::pi) * channelOutput;
            }
            break;
        case BIT_CRUSH:
            {
                const auto levels = juce::jlimit(1, 32, static_cast<int>(drive[channel]));
                const auto step = 2.0f / static_cast<float>(levels);
                for (size_t sample = 0; sample < numSamples; ++sample)
                    output[sample] = std::round(input[sample] / step) * step * channelOutput;
            }
            break;
        case DOWN_SAMPLE:
            {
                const auto period = juce::jmax(1.0f, drive[channel]);
                for (size_t sample = 0; sample < numSamples; ++sample)
                {
                    downSampleCounter += 1.0f;
                    if (downSampleCounter >= period)
                    {
                        downSampleCounter -= period;
                        lastDownSampleValue = input[sample];
                    }
                    output[sample] = lastDownSampleValue * channelOutput;
                }
            }
            break;
        default:
            jassertfalse;
            break;
        }
    }

    if (filterMode == post)
    {
        filter.process(context);
    }
}
