//
// Created by Gabriel Soule on 5/1/24.
//

#ifndef RESONATORSYNTH_H
#define RESONATORSYNTH_H

#pragma once
#include <JuceHeader.h>

class ResonariumProcessor;

class ResonatorSynth : gin::Synthesiser {
public:
    explicit ResonatorSynth(ResonariumProcessor& p);
private:
    ResonariumProcessor& processor;
};



#endif //RESONATORSYNTH_H
