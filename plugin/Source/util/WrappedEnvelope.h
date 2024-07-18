//
// Created by Gabriel Soule on 7/17/24.
//

#ifndef ENVELOPE_H
#define ENVELOPE_H

#include <JuceHeader.h>

#include "../Parameters.h"

class WrappedEnvelope {
public:

    void prepare(const juce::dsp::ProcessSpec& spec)
    {
        internalADSR.setSampleRate(spec.sampleRate);
    }

    template<typename T>
    void updateParameters(T& source)
    {
        internalADSR.setAttack(source.getValue(params.attack));
        internalADSR.setDecay(source.getValue(params.decay));
        internalADSR.setSustainLevel(source.getValue(params.sustain));
        internalADSR.setRelease(source.getValue(params.release));
    }

    void noteOn()
    {
        internalADSR.noteOn();
    }

    void noteOff()
    {
        internalADSR.noteOff();
    }

    void reset()
    {
        internalADSR.reset();
    }

    void process(int numSamples)
    {
        internalADSR.process(numSamples);
    }

    float getOutput()
    {
        return internalADSR.getOutput();
    }

    ADSRParams params;
    gin::ADSR internalADSR;
};



#endif //ENVELOPE_H
