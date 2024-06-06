//
// Created by Gabriel Soule on 5/1/24.
//

#ifndef RESONATORSYNTH_H
#define RESONATORSYNTH_H

#pragma once
#include <JuceHeader.h>

class ResonariumProcessor;

class ResonatorSynth : public gin::Synthesiser {
public:
    explicit ResonatorSynth(ResonariumProcessor& p);
    void prepare(const juce::dsp::ProcessSpec& spec);

    /**
     * Called once during setup. Instructs the synth to access parameters from the processor
     * and distribute them to the appropriate voices and resonators.
     */
    void distributeParameters();
private:
    ResonariumProcessor& processor;
};



#endif //RESONATORSYNTH_H
