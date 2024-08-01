#include "ModalResonatorBank.h"

#include "ResonatorVoice.h"

ModalResonatorBank::ModalResonatorBank(ResonatorVoice& parentVoice, ModalResonatorBankParams params) : ResonatorBank(parentVoice), params(params)
{
    modalBankIndex = params.index;
}

ModalResonatorBank::~ModalResonatorBank()
{
}

void ModalResonatorBank::prepare(const juce::dsp::ProcessSpec& spec)
{
    this->sampleRate = spec.sampleRate;
    this->scratchBuffer = juce::AudioBuffer<float>(spec.numChannels, spec.maximumBlockSize);
    for(int i = 0; i < NUM_MODAL_RESONATORS; i++)
    {
        svfResonators[i].prepare(spec);
    }
}

void ModalResonatorBank::reset()
{
    for(int i = 0; i < NUM_MODAL_RESONATORS; i++)
    {
        svfResonators[i].reset();
    }

}

void ModalResonatorBank::process(juce::dsp::AudioBlock<float>& exciterBlock, juce::dsp::AudioBlock<float>& outputBlock)
{
    juce::dsp::AudioBlock<float> scratchBlock = juce::dsp::AudioBlock<float>(scratchBuffer).getSubBlock(0, (size_t)exciterBlock.getNumSamples());

    float totalGain = 0.0f;
    for(int i = 0; i < NUM_MODAL_RESONATORS; i++)
    {
        if(params.enabled[i]->isOn())
        {
            totalGain += gain[i];
        }
    }

    if (totalGain == 0.0f) return;

    for(int i = 0; i < NUM_MODAL_RESONATORS; i++)
    {
        if(params.enabled[i]->isOn())
        {
            juce::dsp::ProcessContextNonReplacing<float> context(exciterBlock, scratchBlock);
            // resonators[i].process(context);
            svfResonators[i].process(context);
            scratchBlock.multiplyBy(gain[i] * BANDPASS_AMPLITUDE_SCALE);
            outputBlock.add(scratchBlock);
        }
    }
}

void ModalResonatorBank::updateParameters(float newFrequency, int numSamples)
{
    for(int i = 0; i < NUM_MODAL_RESONATORS; i++)
    {
        if(!params.enabled[i]->isOn()) break;

        float newFrequencyOffsetInSemis = voice.getValue(params.harmonicInSemitones[i]);
        float newGain = voice.getValue(params.gain[i]);
        float newDecay = voice.getValue(params.decay[i]);
        gain[i] = newGain;
        if(newGain == 0 || newDecay == 0)
        {
            break;
        }

        if(newFrequencyOffsetInSemis != frequencyOffsets[i]
            || newDecay != decay[i]
            || newFrequency != frequency)
        {
            frequencyOffsets[i] = newFrequencyOffsetInSemis;
            float newFrequencyOffset = std::pow(2.0f, newFrequencyOffsetInSemis / 12.0f);
            gain[i] = newGain;
            decay[i] = newDecay;
            svfResonators[i].setCutoffFrequency<false>(newFrequency * newFrequencyOffset);
            svfResonators[i].setQValue<false>(decay[i] * 800);
            svfResonators[i].update();
        }


    }
}

// std::array<float, 6> ModalResonatorBank::makeResonatorCoefficients(float sampleRate, float frequency, float t60)
// {
//     const float w = 2 * juce::MathConstants<float>::pi * frequency / sampleRate;
//     const float r = std::pow(0.001, 1/(t60 * sampleRate));
//     return std::array<float, 6>{
//         1,
//         0,
//         -1,
//         1,
//         -2 * r * std::cos(w),
//         r * r};
// }
