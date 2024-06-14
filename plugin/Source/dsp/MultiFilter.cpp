#include "MultiFilter.h"
#include "../ResonatorVoice.h"

void MultiFilter::prepare(const juce::dsp::ProcessSpec& spec)
{
    poly = (voice != nullptr);
    this->sampleRate = spec.sampleRate;

    this->freq = 2000.0f;
    this->Q = 0.707f;

    filter.state = juce::dsp::IIR::Coefficients<float>::makeLowPass(sampleRate, freq);
    filter.prepare(spec);

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
    filter.reset();
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

std::array<float, 6> MultiFilter::makeUnityGainBandpass(float sampleRate, float frequency, float Q)
{
    jassert (sampleRate > 0.0);
    jassert (frequency > 0 && frequency <= static_cast<float> (sampleRate * 0.5));
    jassert (Q > 0.0);

    const auto n = 1 / std::tan (juce::MathConstants<float>::pi * frequency / static_cast<float> (sampleRate));
    const auto nSquared = n * n;
    const auto invQ = 1 / Q;
    const auto c1 = 1 / (1 + invQ * n + nSquared);

    return { { c1 * n, 0,
               -c1 * n, 1,
               c1 * 2 * (1 - nSquared),
               c1 * (1 - invQ * n + nSquared) } };
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
    switch (type)
    {
    case MultiFilter::Type::lowpass:
        *filter.state = juce::dsp::IIR::ArrayCoefficients<float>::makeLowPass(sampleRate, freq, Q);
        break;
    case MultiFilter::Type::highpass:
        *filter.state = juce::dsp::IIR::ArrayCoefficients<float>::makeHighPass(sampleRate, freq, Q);
        break;
    case MultiFilter::Type::bandpass:
        *filter.state = makeUnityGainBandpass(sampleRate, freq, Q);
        break;
    case MultiFilter::Type::notch:
        *filter.state = juce::dsp::IIR::ArrayCoefficients<float>::makeNotch(sampleRate, freq, Q);
        break;

    case MultiFilter::Type::allpass:
        *filter.state = juce::dsp::IIR::ArrayCoefficients<float>::makeAllPass(sampleRate, freq, Q);
        break;
    default:
        break;
    }
}
