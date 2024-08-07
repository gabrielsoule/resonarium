//
// Created by Gabriel Soule on 7/27/24.
//

#ifndef NORMALIZEDSVF_H
#define NORMALIZEDSVF_H
#include <JuceHeader.h>

/**
* A normalized multi-mode state variable filter.
*/
template <typename SampleType, size_t maxChannelCount = chowdsp::defaultChannelCount>
class NormalizedSVF : public chowdsp::StateVariableFilter<SampleType, chowdsp::StateVariableFilterType::MultiMode, maxChannelCount>
{
public:
    using BaseFilter = chowdsp::StateVariableFilter<SampleType, chowdsp::StateVariableFilterType::MultiMode, maxChannelCount>;
    using NumericType = typename BaseFilter::NumericType;

    // Constructor
    NormalizedSVF() : BaseFilter() {}

    // Override setMode without the complex templating
    void setMode(NumericType mode)
    {
        this->lowpassMult = (NumericType) 1 - (NumericType) 2 * juce::jmin ((NumericType) 0.5, mode);
        this->bandpassMult = (NumericType) 1 - std::abs ((NumericType) 2 * (mode - (NumericType) 0.5));
        this->highpassMult = (NumericType) 2 * juce::jmax ((NumericType) 0.5, mode) - (NumericType) 1;
    }

    // You can add other methods or override other functions as needed
};



#endif //NORMALIZEDSVF_H
