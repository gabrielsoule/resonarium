//
// Created by Gabriel Soule on 5/8/24.
//

#ifndef RESONATORBANK_H
#define RESONATORBANK_H
#include "Resonator.h"

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

};



#endif //RESONATORBANK_H
