#include "MultiFilter.h"
#include "../ResonatorVoice.h"

void MultiFilter::prepare(const juce::dsp::ProcessSpec& spec)
{
    poly = (voice != nullptr);
    this->sampleRate = spec.sampleRate;

    this->freq = 2000.0f;
    this->Q = 0.707f;

    coefficients = juce::dsp::IIR::ArrayCoefficients<float>::makeLowPass(sampleRate, freq, Q);
    *filterL.coefficients = coefficients;
    *filterR.coefficients = coefficients;

    if (params.frequency == nullptr || params.resonance == nullptr || params.type == nullptr)
    {
        useHostedParams = false;
    }
    else
    {
        useHostedParams = true;
    }
}

void MultiFilter::reset()
{
    filterL.reset();
    filterR.reset();
}

void MultiFilter::setType(Type type)
{
    this->type = type;
    updateFlag = true;
}

void MultiFilter::setParameters(float frequency, float q)
{
    this->freq = frequency;
    this->Q = q;
    updateFlag = true;
}

std::array<float, 6> MultiFilter::makeHighGainBandpass(float sampleRate, float frequency, float Q)
{
    jassert(sampleRate > 0.0);
    jassert(frequency > 0 && frequency <= static_cast<float> (sampleRate * 0.5));
    jassert(Q > 0.0);

    const auto n = 1 / std::tan(juce::MathConstants<float>::pi * frequency / static_cast<float>(sampleRate));
    const auto nSquared = n * n;
    const auto invQ = 1 / Q;
    const auto c1 = 1 / (1 + invQ * n + nSquared);

    return {
        {
            c1 * n, 0,
            -c1 * n, 1,
            c1 * 2 * (1 - nSquared),
            c1 * (1 - invQ * n + nSquared)
        }
    };
}

float MultiFilter::processSample(int channel, float sample) noexcept
{
    if (channel == 0)
    {
        return filterL.processSample(sample);
    }
    else if (channel == 1)
    {
        return filterR.processSample(sample);
    }
    else
    {
        jassertfalse;
        return -1;
    }
}

void MultiFilter::process(juce::dsp::AudioBlock<float>& block) noexcept
{
    if(type == none) return; //no-op
    size_t numChannels = block.getNumChannels();
    juce::dsp::AudioBlock<float> leftBlock = block.getSingleChannelBlock(0);
    juce::dsp::ProcessContextReplacing<float> leftContext(leftBlock);
    filterL.process(leftContext);

    if (numChannels > 1)
    {
        juce::dsp::AudioBlock<float> rightBlock = block.getSingleChannelBlock(1);
        juce::dsp::ProcessContextReplacing<float> rightContext(rightBlock);
        filterR.process(rightContext);
    }
}

void MultiFilter::updateParameters()
{
    if (useHostedParams)
    {
        this->type = static_cast<Type>(static_cast<int>(params.type->getProcValue()));
        if (poly)
        {
            freq = voice->getValue(params.frequency);
            Q = voice->getValue(params.resonance);
        }
        else
        {
            freq = params.frequency->getProcValue();
            Q = params.resonance->getProcValue();
        }
    }

    if (type == none) return; //no-op

    //TODO add optimization via updateFlag to only compute new coefficients when necessary
    if (!useHostedParams && !updateFlag) return; //
    switch (type)
    {
    case MultiFilter::Type::lowpass:
        coefficients = juce::dsp::IIR::ArrayCoefficients<float>::makeLowPass(sampleRate, freq, Q);
        break;
    case MultiFilter::Type::highpass:
        coefficients = juce::dsp::IIR::ArrayCoefficients<float>::makeHighPass(sampleRate, freq, Q);
        break;
    case MultiFilter::Type::bandpass:
        if (!normalize)
        {
            coefficients = makeHighGainBandpass(sampleRate, freq, Q);
        }
        else
        {
            coefficients = juce::dsp::IIR::ArrayCoefficients<float>::makeBandPass(sampleRate, freq, Q);
        }
        break;
    case MultiFilter::Type::notch:
        coefficients = juce::dsp::IIR::ArrayCoefficients<float>::makeNotch(sampleRate, freq, Q);
        break;
    case MultiFilter::Type::allpass:
        coefficients = juce::dsp::IIR::ArrayCoefficients<float>::makeAllPass(sampleRate, freq, Q);
        break;
    default:
        jassertfalse;
        break;
    }

    *filterL.coefficients = coefficients;
    *filterR.coefficients = coefficients;

    updateFlag = false;
}
