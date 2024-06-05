//
// Created by Gabriel Soule on 5/20/24.
//

#ifndef FILTERS_H
#define FILTERS_H
#include <JuceHeader.h>


/**
 * A first order allpass filter based on a lattice structure.
 *
 */
class DispersionFilter {
public:
 float processSample(float input);
 void prepare(juce::dsp::ProcessSpec spec);
 void reset();
 void setDispersionAmount(float amount);

 float state[2];
 float c;
 float s;
};



#endif //FILTERS_H
