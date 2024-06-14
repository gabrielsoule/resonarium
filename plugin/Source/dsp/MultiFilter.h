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
* is null, it will operate in monophonic mode automatically,
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

    MultiFilter() : type(none) {}
    MultiFilter(MultiFilterParams params) : type(none), params(params) {}
    MultiFilter(ResonatorVoice* voice, MultiFilterParams params) : type(none), voice(voice), params(params) {}
    void prepare(const juce::dsp::ProcessSpec& spec);
    void reset();
    void setType(Type type);
    void setParameters(float frequency, float q);
    static std::array<float, 6> makeUnityGainBandpass(float sampleRate, float frequency, float Q);
    template <typename ProcessContext>
    void process (const ProcessContext& context) noexcept
    {
        if (type != none)
        {
            filter.process(context);
        }
    }

    /**
    * Updates the filter coefficients automatically by querying the hosted parameter struct.
    */
    void updateParameters();

    juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>, juce::dsp::IIR::Coefficients<float>> filter;
    Type type;
    float freq;
    float Q;
    float sampleRate;
    bool poly; // true if the filter is a polyphonic filter, i.e. voice is not null
    bool useHostedParams; //true if the filter should retrieve parameters from the hosted parameter struct
    bool updateFlag = false;
    ResonatorVoice* voice;
    MultiFilterParams params;
};


#endif //MULTIFILTER_H
