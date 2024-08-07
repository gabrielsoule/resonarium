//
// Created by Gabriel Soule on 7/27/24.
//

#ifndef NORMALIZEDSVF_H
#define NORMALIZEDSVF_H
#include <JuceHeader.h>

/**
* A normalized multi-mode state variable filter.
*/
template <typename SampleType, size_t maxChannelCount>
class NormalizedSVF : chowdsp::SVFMultiMode<SampleType, maxChannelCount> {

};



#endif //NORMALIZEDSVF_H
