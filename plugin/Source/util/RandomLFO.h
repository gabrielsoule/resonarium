//
// Created by Gabriel Soule on 7/10/24.
//

#ifndef RANDOMLFO_H
#define RANDOMLFO_H
#include "../Parameters.h"

/**
* A versatile source of randomness that can be used as a modulator signal.
* Produces either a monophonic or a polyphonic output.
* Can be supplied with an optional RandomLFOParameters struct to control its behavior.
*/
class RandomLFO {
public:
    explicit RandomLFO();
    explicit RandomLFO(int seed);
    explicit RandomLFO(RandomLFOParams params);
    void prepare(const juce::dsp::ProcessSpec& spec);
    void updateParameters();
    /**
     * Returns the value of the random LFO after numSamples have been processed,
     * and updates its internal state accordingly.
     */
    float process(int numSamples);
    RandomLFOParams params;
    juce::Random rand;
};



#endif //RANDOMLFO_H
