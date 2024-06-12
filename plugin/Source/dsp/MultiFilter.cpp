#include "MultiFilter.h"
#include "../ResonatorVoice.h"

MultiFilter::MultiFilter() : type(MultiFilter::Type::none)
{
}

void MultiFilter::prepare(const juce::dsp::ProcessSpec& spec, ResonatorVoice* voice)
{
    poly = (voice != nullptr);
    this->voice = voice;
    this->sampleRate = spec.sampleRate;

    this->freq = 2000.0f;
    this->q = 0.707f;

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
    this->q = q;
    updateFlag = true;
}

void MultiFilter::updateParameters()
{
    if (type == none) return; //no-op

    if (useHostedParams)
    {
        type = static_cast<Type>(static_cast<int>(params.type->getProcValue()));
        if (poly)
        {
            freq = voice->getValue(params.frequency);
            q = voice->getValue(params.resonance);
        }
        else
        {
            freq = params.frequency->getProcValue();
            q = params.resonance->getProcValue();
        }
    }

    //TODO add optimization via updateFlag to only compute new coefficients when necessary
    switch (type)
    {
    case MultiFilter::Type::lowpass:
        *filter.state = juce::dsp::IIR::ArrayCoefficients<float>::makeLowPass(sampleRate, freq, q);
        break;
    case MultiFilter::Type::highpass:
        *filter.state = juce::dsp::IIR::ArrayCoefficients<float>::makeHighPass(sampleRate, freq, q);
        break;
    case MultiFilter::Type::bandpass:
        *filter.state = juce::dsp::IIR::ArrayCoefficients<float>::makeBandPass(sampleRate, freq, q);
        break;
    case MultiFilter::Type::notch:
        *filter.state = juce::dsp::IIR::ArrayCoefficients<float>::makeNotch(sampleRate, freq, q);
        break;
    // case MultiFilter::Type::lowshelf:
    //     *filter.state = juce::dsp::IIR::ArrayCoefficients<float>::makeLowShelf(sampleRate, freq, q);
    //     break;
    // case MultiFilter::Type::highshelf:
    //     *filter.state = juce::dsp::IIR::ArrayCoefficients<float>::makeHighShelf(sampleRate, freq, q);
    //     break;
    // case MultiFilter::Type::peak:
    //     *filter.state = juce::dsp::IIR::ArrayCoefficients<float>::makePeakFilter(sampleRate, freq, q, 1.5f);
    //     break;
    case MultiFilter::Type::allpass:
        *filter.state = juce::dsp::IIR::ArrayCoefficients<float>::makeAllPass(sampleRate, freq, q);
        break;
    default:
        break;
    }
}
