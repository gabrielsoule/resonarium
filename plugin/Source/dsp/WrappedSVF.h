#ifndef WRAPPEDSVF_H
#define WRAPPEDSVF_H

#include <JuceHeader.h>

#include "../Parameters.h"

/**
* Wraps a state variable filter in a convienient class that contains its own parameters struct,
* along with a simple method for updating the filter coefficients.
*
* TODO Add stereo modulation support
*/
class WrappedSVF
{
public:
    explicit WrappedSVF(SVFParams params) : params(params) {}

    void prepare(const juce::dsp::ProcessSpec& spec);
    void reset();

    template <typename T>
    void updateParameters(T& source)
    {
        svfL.updateParameters(source.getValue(params.cutoff, 0),
                              source.getValue(params.resonance, 0),
                              source.getValue(params.mode, 0));
        svfR.updateParameters(source.getValue(params.cutoff, 1),
                              source.getValue(params.resonance, 1),
                              source.getValue(params.mode, 1));
    }

    template <typename ProcessContext>
    void process(ProcessContext& context)
    {
        juce::dsp::AudioBlock<float> blockL = context.getOutputBlock().getSingleChannelBlock(0);
        juce::dsp::AudioBlock<float> blockR = context.getOutputBlock().getSingleChannelBlock(1);
        juce::dsp::ProcessContextReplacing<float> contextL(blockL);
        juce::dsp::ProcessContextReplacing<float> contextR(blockR);
        svfL.process(contextL);
        svfR.process(contextR);
    }

    chowdsp::SVFMultiMode<float, 1, false> svfL;
    chowdsp::SVFMultiMode<float, 1, false> svfR;
    SVFParams params;
};


#endif //WRAPPEDSVF_H
