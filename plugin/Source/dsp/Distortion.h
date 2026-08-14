#ifndef DISTORTION_H
#define DISTORTION_H
#include "../Parameters.h"
#include "StateVariableFilter.h"

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
        for (int channel = 0; channel < 2; ++channel)
        {
            const auto nextDrive = source.getValue(params.drive, channel);
            if (!controlsInitialised || nextDrive != drive[channel])
            {
                drive[channel] = nextDrive;
                scaledDrive[channel] = std::pow(10.0f, nextDrive / 20.0f);
            }

            const auto nextOutputGain = source.getValue(params.outputGain, channel);
            if (!controlsInitialised || nextOutputGain != outputGain[channel])
            {
                outputGain[channel] = nextOutputGain;
                scaledOutputGain[channel] = std::pow(10.0f, nextOutputGain / 20.0f);
            }
        }
        controlsInitialised = true;
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
    float scaledDrive[2] = {1.0f, 1.0f};
    float scaledOutputGain[2] = {1.0f, 1.0f};
    bool controlsInitialised = false;
    FilterMode filterMode = disabled;
    resonarium::StateVariableFilter<float, 2, false> filter;

    // State variables for downsampling
    float lastDownSampleValue = 0.0f;
    float downSampleCounter = 0.0f;
};

#endif // DISTORTION_H
