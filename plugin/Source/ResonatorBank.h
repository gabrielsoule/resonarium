#ifndef RESONATORBANK_H
#define RESONATORBANK_H
#include "Resonator.h"

class ResonatorVoice;
/**
 * A bank of several Resonators, with support for different intra-resonator feedback modes.
 */
class ResonatorBank
{
public:
    enum CouplingMode
    {
        PARALLEL,
        INTERLINKED,
        CASCADE,
    };

    ResonatorBank(ResonatorVoice& parentVoice, ResonatorBankParams params);
    ~ResonatorBank();
    void process(juce::dsp::AudioBlock<float>& exciterBlock, juce::dsp::AudioBlock<float>& outputBlock);
    void reset();
    void prepare(const juce::dsp::ProcessSpec& spec);
    void updateParameters(float newFrequency);
    void setFeedbackMode(CouplingMode newMode);

    CouplingMode couplingMode;
    float frequency;
    float sampleRate;
    juce::OwnedArray<Resonator> resonators;

    juce::dsp::IIR::Filter<float> couplingFilter;

    //Pointer to the voice that owns this ResonatorBank; awkwardly required for polyphonic modulation via ModMatrix
    ResonatorVoice& voice;

    //Pointers to the relevant parameters controlling this ResonatorBank
    ResonatorBankParams params;
};

#endif //RESONATORBANK_H
