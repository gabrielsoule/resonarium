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

    CouplingMode couplingMode;
    float frequency;
    float sampleRate;
    juce::OwnedArray<Resonator> resonators;

    juce::dsp::IIR::Filter<float> couplingFilter;

    //Pointers to the relevant parameters controlling this WaveguideResonatorBank
    WaveguideResonatorBankParams params;
    juce::dsp::AudioBlock<float> scratchBuffer;
};

#endif //WAVEGUIDERESONATORBANK_H