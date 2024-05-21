//
// Created by Gabriel Soule on 5/8/24.
//

#ifndef RESONATORBANK_H
#define RESONATORBANK_H
#include "Resonator.h"

#define NUM_RESONATORS 1

/**
 * A bank of several Resonators, with support for different intra-resonator feedback modes.
 */
class ResonatorBank {
public:


    enum CouplingMode
    {
        PARALLEL,
        CASCADE,
        INTERLINKED,
        RANDOM
    };

    ResonatorBank();
    ~ResonatorBank();

    float processSample(float input);
    void reset();
    void prepare(const juce::dsp::ProcessSpec& spec);
    void setFrequency(float newFrequency);
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

};



#endif //RESONATORBANK_H
