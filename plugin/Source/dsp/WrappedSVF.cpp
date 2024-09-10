#include "WrappedSVF.h"

void WrappedSVF::prepare(const juce::dsp::ProcessSpec& spec)
{
    svfL.prepare({ spec.sampleRate, spec.maximumBlockSize, 1 });
    svfR.prepare({ spec.sampleRate, spec.maximumBlockSize, 1 });
}

void WrappedSVF::reset()
{
    svfL.reset();
    svfR.reset();
}
