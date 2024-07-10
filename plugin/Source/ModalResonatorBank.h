#ifndef MODALRESONATORBANK_H
#define MODALRESONATORBANK_H
#include "Parameters.h"
#include "ResonatorBank.h"

//the resonant bandpass filters have very high gain right out of the box
#define BANDPASS_AMPLITUDE_SCALE 0.1f


class ModalResonatorBank : public ResonatorBank {
public:
    ModalResonatorBank(ResonatorVoice& parentVoice, ModalResonatorBankParams params);
    ~ModalResonatorBank();
    void prepare(const juce::dsp::ProcessSpec& spec) override;
    void reset() override;
    void process(juce::dsp::AudioBlock<float>& exciterBlock, juce::dsp::AudioBlock<float>& outputBlock) override;
    void updateParameters(float newFrequency) override;

    float frequency;
    float sampleRate;

    ModalResonatorBankParams params;
    int modalBankIndex = -1;
    juce::AudioBuffer<float> scratchBuffer;
    chowdsp::StateVariableFilter<float, chowdsp::StateVariableFilterType::Bandpass, 2> svfResonators[NUM_MODAL_RESONATORS];
    float frequencyOffsets[NUM_MODAL_RESONATORS];
    float gain[NUM_MODAL_RESONATORS];
    float decay[NUM_MODAL_RESONATORS];

    // static std::array<float, 6> makeResonatorCoefficients(float sampleRate, float frequency, float t60);
};


#endif //MODALRESONATORBANK_H
