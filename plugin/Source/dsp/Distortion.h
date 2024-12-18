#ifndef DISTORTION_H
#define DISTORTION_H
#include "../Parameters.h"

class Distortion
{
public:
    enum DistortionMode
    {
        SOFT_CLIP,
        HARD_CLIP,
        LINEAR_FOLD,
        SIN_FOLD,
        BIT_CRUSH,
        DOWN_SAMPLE,
        NUM_MODES
    };

    enum FilterMode
    {
        disabled,
        pre,
        post
    };

    explicit Distortion(DistortionParams params);

    void prepare(const juce::dsp::ProcessSpec& spec);

    void reset();

    template <typename T>
    void updateParameters(T& source)
    {
        distortionMode = static_cast<DistortionMode>(params.distortionMode->getProcValue());
        drive[0] = source.getValue(params.drive, 0);
        drive[1] = source.getValue(params.drive, 1);
        outputGain[0] = source.getValue(params.outputGain, 0);
        outputGain[1] = source.getValue(params.outputGain, 1);
        filterMode = static_cast<FilterMode>(params.filterMode->getProcValue());
        filter.updateParameters(source.getValue(params.cutoff), source.getValue(params.resonance),
                                source.getValue(params.filterMode));
    }

    void process(juce::dsp::ProcessContextReplacing<float> context);

private:
    DistortionParams params;

    DistortionMode distortionMode;
    float drive[2] = {1.0f, 1.0f};
    float outputGain[2] = {1.0f, 1.0f};
    FilterMode filterMode = disabled;
    chowdsp::SVFMultiMode<float, 2> filter;

    // State variables for downsampling
    float lastDownSampleValue = 0.0f;
    float downSampleCounter = 0.0f;
};

#endif // DISTORTION_H
