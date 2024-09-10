#ifndef STEREOLFOWRAPPER_H
#define STEREOLFOWRAPPER_H


#include <JuceHeader.h>
#include "../Parameters.h"
/**
* This class wraps around (two copies of) Gin's excellent LFO to provide stereo functionality.

*/
class StereoLFOWrapper {
public:

    StereoLFOWrapper() = default;

    void prepare(const juce::dsp::ProcessSpec& spec);

    template<typename T>
    void updateParameters(T& source, float frequency)
    {
        this->ginParams.frequency = frequency;
        this->ginParams.waveShape = static_cast<gin::LFO::WaveShape>(params.wave->getProcValue());
        this->ginParams.phase = source.getValue(params.phase);
        this->ginParams.offset = source.getValue(params.offset);
        this->ginParams.depth = source.getValue(params.depth);
        this->ginParams.delay = source.getValue(params.delay);
        this->ginParams.fade = source.getValue(params.fade);

        left.setParameters(ginParams);
        ginParams.phase += source.getValue(params.stereo);
        right.setParameters(ginParams);
    }
    void reset();
    void noteOn(float phase);
    void process(int numSamples);
    float getOutput(int channel);
    float getOutputUnclamped(int channel);
    float getCurrentPhase(int channel);

    gin::LFO left;
    gin::LFO right;
    LFOParams params;
    gin::LFO::Parameters ginParams;

    gin::LFO::WaveShape waveShape = gin::LFO::WaveShape::sine;
    float frequency = 1.0f;
    float phase = 0.0f;
    float offset = 0.0f;
    float depth = 1.0f;
    float delay = 0.0f;
    float fade = 0.0f;
    float stereo = 0.0f;

};



#endif //STEREOLFOWRAPPER_H
