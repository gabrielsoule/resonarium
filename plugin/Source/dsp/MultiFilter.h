#ifndef MULTIFILTER_H
#define MULTIFILTER_H

#include <JuceHeader.h>
#include "../Parameters.h"

class ResonatorVoice;

/**
* Light wrapper over Juce::IIRFilter to easily enable multiple filter types in a single class,
* along with an optional set of host-controlled parameters.
*

* The MultiFilter is marginally clever: if the voice pointer
* is null, it will operate in monophonic mode automatically,
* and if any of the parameter pointers are null, then it will operate
* without relying on the hosted parameters. It will operate in mono or stereo
* depending on the number of channels in the input block.
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

    MultiFilter() : type(none), normalize(false) {}
    MultiFilter(bool normalize) : type(none), normalize(normalize) {}
    MultiFilter(MultiFilterParams params, bool normalize) : type(none), params(params), normalize(normalize)
    {
        params.frequency = nullptr;
        params.resonance = nullptr;
        params.type = nullptr;
    }
    MultiFilter(ResonatorVoice* voice, MultiFilterParams params, bool normalize) : type(none), voice(voice), params(params), normalize(normalize) {}
    void prepare(const juce::dsp::ProcessSpec& spec);
    void reset();
    void setType(Type type);
    void setParameters(float frequency, float q);
    static std::array<float, 6> makeHighGainBandpass(float sampleRate, float frequency, float Q);
    float processSample(int channel, float sample) noexcept;
    void process (juce::dsp::AudioBlock<float>& block) noexcept;



    /**
    * Updates the filter coefficients automatically by querying the hosted parameter struct.
    */
    void updateParameters();

    // juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>, juce::dsp::IIR::Coefficients<float>> filter;
    juce::dsp::IIR::Filter<float> filterL;
    juce::dsp::IIR::Filter<float> filterR;
    Type type;
    float freq;
    float Q;
    float sampleRate;
    bool poly; // true if the filter is a polyphonic filter, i.e. voice is not null
    bool useHostedParams; //true if the filter should retrieve parameters from the hosted parameter struct
    bool updateFlag = false;
    ResonatorVoice* voice;
    MultiFilterParams params;
    bool normalize; //true if the bandpass filter should be unity gain, or boosted by Q
    std::array<float, 6> coefficients;
};


#endif //MULTIFILTER_H
