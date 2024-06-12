#ifndef MULTIFILTER_H
#define MULTIFILTER_H

#include <JuceHeader.h>

#include "../Parameters.h"

class ResonatorVoice;

/**
* Light wrapper over Juce::IIRFilter to easily enable multiple filter types in a single class,
* along with a set of host-controlled parameters.
*

* The MultiFilter is marginally clever: if the voice pointer
* is null, it will operate in mono mode automatically,
* and if any of the parameter pointers are null, then it will operate
* without relying on the hosted parameters.
*/
class MultiFilter
{
public:

    enum Type
    {
        none = 0,
        lowpass,
        highpass,
        bandpass,
        notch,
        allpass,
    };

    MultiFilter();

    void prepare(const juce::dsp::ProcessSpec& spec, ResonatorVoice* voice);

    void reset();

    void setType(Type type);

    void setParameters(float frequency, float q);

    template <typename ProcessContext>
    void process (const ProcessContext& context) noexcept
    {
        if (type != none) filter.process(context);
    }

    /**
    * Updates the filter coefficients automatically by queryiung the hosted parameter struct.
    */
    void updateParameters();

    ResonatorVoice* voice;
    juce::dsp::IIR::ArrayCoefficients<float> coefficients;
    // juce::dsp::IIR::Filter<float> filter;
    juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>, juce::dsp::IIR::Coefficients<float>> filter;
    MultiFilterParams params;
    Type type;
    float freq;
    float q;
    float sampleRate;
    bool poly; // true if the filter is a polyphonic filter, i.e. voice is not null
    bool useHostedParams; //true if the filter should retrieve parameters from the hosted parameter struct
    bool updateFlag = false;
};


#endif //MULTIFILTER_H
