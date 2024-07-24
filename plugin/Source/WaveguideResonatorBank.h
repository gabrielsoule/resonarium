#ifndef WAVEGUIDERESONATORBANK_H
#define WAVEGUIDERESONATORBANK_H
#include "defines.h"
#include "Resonator.h"
#include "ResonatorBank.h"
#include "StereoResonator.h"

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
    // juce::OwnedArray<Resonator> resonators;
    juce::OwnedArray<StereoResonator> resonators;
    juce::dsp::IIR::Filter<float> couplingFilter;
    juce::dsp::FIR::Filter<float> couplingFilterFIR;
    juce::dsp::FIR::Coefficients<float>::Ptr couplingCoefficientsFIR;
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Lagrange3rd> firDelays[NUM_WAVEGUIDE_RESONATORS];

};

#endif //WAVEGUIDERESONATORBANK_H
