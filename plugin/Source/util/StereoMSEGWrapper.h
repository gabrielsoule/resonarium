#ifndef STEREOMSEGWRAPPER_H
#define STEREOMSEGWRAPPER_H

#include <JuceHeader.h>

#include "../Parameters.h"

class StereoMSEGWrapper
{
public:
    StereoMSEGWrapper(MSEGParams params) : params(params), left(*params.msegData), right(*params.msegData)
    {
        DBG("C...");
    }

    void prepare(const juce::dsp::ProcessSpec& spec);

    template <typename T>
    void updateParameters(T& source, float frequency)
    {
        this->ginParams.frequency = frequency;
        this->ginParams.phase = source.getValue(params.phase);
        this->ginParams.offset = source.getValue(params.offset);
        this->ginParams.depth = source.getValue(params.depth);
        // this->ginParams.delay = source.getValue(params.delay);
        this->ginParams.fade = source.getValue(params.fade);
        this->ginParams.loop = true;

        left.setParameters(ginParams);
        right.setParameters(ginParams);
    }

    void reset();
    void noteOn(float phase = -1);
    void process(int numSamples);
    float getOutput(int channel);
    float getOutputUnclamped(int channel);
    float getCurrentPhase(int channel);

    gin::MSEG::Data leftData;
    gin::MSEG::Data rightData;
    gin::MSEG left;
    gin::MSEG right;
    MSEGParams params;
    gin::MSEG::Parameters ginParams;

    float frequency = 1.0f;
    float phase = 0.0f;
    float offset = 0.0f;
    float depth = 1.0f;
    float delay = 0.0f;
    float fade = 0.0f;
    float stereo = 0.0f;
};


#endif //STEREOMSEGWRAPPER_H