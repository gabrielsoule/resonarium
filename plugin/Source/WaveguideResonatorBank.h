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
    void updateParameters(float newFrequency, int numSamples) override;
    void setFeedbackMode(CouplingMode newMode);

    WaveguideResonatorBankParams params;
    int waveguideBankIndex = -1;
    CouplingMode couplingMode;
    float frequency;
    float sampleRate;
    juce::OwnedArray<StereoResonator> resonators;
    juce::dsp::IIR::Coefficients<float>::Ptr dcBlockerCoefficients;
    // std::array<juce::dsp::IIR::Filter<float>, NUM_WAVEGUIDE_RESONATORS> dcBlockersL;
    juce::dsp::IIR::Filter <float> dcBlockersL[NUM_WAVEGUIDE_RESONATORS];
    // std::array<juce::dsp::IIR::Filter<float>, NUM_WAVEGUIDE_RESONATORS> dcBlockersR;
    juce::dsp::IIR::Filter <float> dcBlockersR[NUM_WAVEGUIDE_RESONATORS];

    // chowdsp::SVFLowpass<float, 2> testCascadeFilters[NUM_WAVEGUIDE_RESONATORS];
    chowdsp::SVFMultiMode<float, NUM_WAVEGUIDE_RESONATORS> cascadeFilterL;
    float cascadeFilterCutoffL;
    float cascadeFilterResonanceL;
    float cascadeFilterModeL;
    float cascadeFilterNormalizationScalarL;
    chowdsp::SVFMultiMode<float, NUM_WAVEGUIDE_RESONATORS> cascadeFilterR;
    float cascadeFilterCutoffR;
    float cascadeFilterResonanceR;
    float cascadeFilterModeR;
    float cascadeFilterNormalizationScalarR;

    //experimental shit
    juce::dsp::IIR::Filter<float> couplingFilter;
    juce::dsp::FIR::Filter<float> couplingFilterFIR;
    juce::dsp::FIR::Coefficients<float>::Ptr couplingCoefficientsFIR;
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Lagrange3rd> firDelays[NUM_WAVEGUIDE_RESONATORS];


};

#endif //WAVEGUIDERESONATORBANK_H
