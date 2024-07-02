#ifndef WAVEGUIDERESONATORBANK_H
#define WAVEGUIDERESONATORBANK_H
#include "Resonator.h"
#include "ResonatorBank.h"

class ResonatorVoice;
/**
 * A bank of several Resonators, with support for different intra-resonator feedback modes.
 */
class WaveguideResonatorBank : public ResonatorBank
{
public:
    enum CouplingMode
    {
        PARALLEL,
        INTERLINKED,
        CASCADE,
    };

    WaveguideResonatorBank(ResonatorVoice& parentVoice, WaveguideResonatorBankParams params);
    ~WaveguideResonatorBank();
    void process(juce::dsp::AudioBlock<float>& exciterBlock, juce::dsp::AudioBlock<float>& outputBlock) override;
    void reset() override;
    void prepare(const juce::dsp::ProcessSpec& spec) override;
    void updateParameters(float newFrequency) override;
    void setFeedbackMode(CouplingMode newMode);

    WaveguideResonatorBankParams params;
    int waveguideBankIndex = -1;
    juce::dsp::AudioBlock<float> scratchBuffer;
    CouplingMode couplingMode;
    float frequency;
    float sampleRate;
    juce::OwnedArray<Resonator> resonators;
    juce::dsp::IIR::Filter<float> couplingFilter;

};

#endif //WAVEGUIDERESONATORBANK_H
