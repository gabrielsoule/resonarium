//
// Created by Gabriel Soule on 5/8/24.
//

#ifndef RESONATORBANK_H
#define RESONATORBANK_H
#include "Resonator.h"
#include "defines.h"

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
        CASCADE,
        INTERLINKED,
        INTERLINKED2,
        RANDOM
    };

    ResonatorBank(ResonatorVoice& parentVoice);
    ~ResonatorBank();

    float processSample(float input);
    void reset();
    void prepare(const juce::dsp::ProcessSpec& spec);
    void updateParameters(float newFrequency);
    void setFeedbackMode(CouplingMode newMode);

    CouplingMode couplingMode;
    float frequency;
    float sampleRate;
    juce::OwnedArray<Resonator> resonators;

    // for use in intra-resonator feedback.
    // stores each resonator's last output times its feedback gain.
    // these will not equal the audible output samples,
    // since the resonators have different gain for feedback vs DAW output.
    // for example, a resonator can hav
    float lastResonatorOutputs[NUM_RESONATORS];
    float lastOutput = 0.0f;
    juce::dsp::IIR::Filter<float> couplingFilter;

    //Pointers to the relevant parameters controlling this ResonatorBank
    ResonatorBankParams params;

    //Pointer to the voice that owns this ResonatorBank; awkwardly required for polyphonic modulation via ModMatrix
    ResonatorVoice& voice;
};

#endif //RESONATORBANK_H
