#ifndef WAVEGUIDERESONATORBANK_H
#define WAVEGUIDERESONATORBANK_H
#include "defines.h"
#include "GlobalState.h"
#include "ResonatorBank.h"
#include "StereoResonator.h"
#include <chowdsp_filters/chowdsp_filters.h>

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
        COUPLED,
        CASCADE,
        COUPLED_FLTR,
    };

    WaveguideResonatorBank(GlobalState& state, ResonatorVoice& parentVoice, WaveguideResonatorBankParams params);
    ~WaveguideResonatorBank();
    void process(
        juce::dsp::AudioBlock<float>& exciterBlock,
        juce::dsp::AudioBlock <float>& previousResonatorBankBlock);
    void reset();
    void prepare(const juce::dsp::ProcessSpec& spec);
    void updateParameters(float newFrequency, int numSamples);
    void setFeedbackMode(CouplingMode newMode);

    GlobalState& state;
    ResonatorVoice& voice;
    WaveguideResonatorBankParams params;
    int index = -1;
    CouplingMode couplingMode = PARALLEL;
    float frequency = 44100.0f;
    float sampleRate = 440.0f;
    juce::OwnedArray<StereoResonator> resonators;

    float previousResonatorBankMix = 0.0f; //how much of the previous resonator bank's output should we mix in?
    float exciterMix = 1.0f; //how much of the exciter signal should we mix in?
    float inputGain = 1.0f;
    float outputGain = 1.0f;

    //below is for cascade mode
    juce::dsp::IIR::Coefficients<float>::Ptr dcBlockerCoefficients;
    juce::dsp::IIR::Filter <float> dcBlockersL[NUM_RESONATORS];
    juce::dsp::IIR::Filter <float> dcBlockersR[NUM_RESONATORS];
    chowdsp::SVFMultiMode<float, NUM_RESONATORS, false> cascadeFilterL;
    chowdsp::SVFMultiMode<float, NUM_RESONATORS, false> cascadeFilterR;

    chowdsp::SVFMultiMode<float, NUM_RESONATORS, true> testInterlinkedFilterL;
    chowdsp::SVFMultiMode<float, NUM_RESONATORS, true> testInterlinkedFilterR;

    std::array<chowdsp::LinkwitzRileyFilter<float, 2>, NUM_RESONATORS> crossoverFilters;
    chowdsp::LinkwitzRileyFilter<float, 2> coupledCrossoverFilter;
};

#endif //WAVEGUIDERESONATORBANK_H
