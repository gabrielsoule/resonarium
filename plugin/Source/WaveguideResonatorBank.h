#ifndef WAVEGUIDERESONATORBANK_H
#define WAVEGUIDERESONATORBANK_H
#include "defines.h"
#include "ResonatorBank.h"
#include "StereoResonator.h"

class ResonatorVoice;
/**
 * A bank of several Resonators, with support for different intra-resonator feedback modes.
 */
class WaveguideResonatorBank
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
    void process(
        juce::dsp::AudioBlock<float>& exciterBlock,
        juce::dsp::AudioBlock <float>& previousResonatorBankBlock);
    void reset();
    void prepare(const juce::dsp::ProcessSpec& spec);
    void updateParameters(float newFrequency, int numSamples);
    void setFeedbackMode(CouplingMode newMode);

    ResonatorVoice& voice;
    WaveguideResonatorBankParams params;
    int index = -1;
    CouplingMode couplingMode;
    float frequency;
    float sampleRate;
    juce::OwnedArray<StereoResonator> resonators;

    float previousResonatorMix = 0.0f; //how much of the previous resonator bank's output should we mix in?
    float exciterMix = 1.0f; //how much of the exciter signal should we mix in?

    //the below is for cascade mode
    juce::dsp::IIR::Coefficients<float>::Ptr dcBlockerCoefficients;
    juce::dsp::IIR::Filter <float> dcBlockersL[NUM_WAVEGUIDE_RESONATORS];
    juce::dsp::IIR::Filter <float> dcBlockersR[NUM_WAVEGUIDE_RESONATORS];
    chowdsp::SVFMultiMode<float, NUM_WAVEGUIDE_RESONATORS, true> cascadeFilterL;
    chowdsp::SVFMultiMode<float, NUM_WAVEGUIDE_RESONATORS, true> cascadeFilterR;
};

#endif //WAVEGUIDERESONATORBANK_H
