//
// Created by Gabriel Soule on 6/18/24.
//

#ifndef EQTHREE_H
#define EQTHREE_H

#include <JuceHeader.h>
class ResonatorVoice;
/**
* A three-band EQ th
*/
class EQThree {
public:
  EQThree (int order) : order(order) {}
  EQThree (ResonatorVoice* voice, int order) : voice(voice), order(order) {}


  int getOrder();

  ResonatorVoice* voice;
  // juce::OwnedArray<chowdsp::StateVariableFilter<float, chowdsp::StateVariableFilterType::>>

private:
  int order;

};



#endif //EQTHREE_H
