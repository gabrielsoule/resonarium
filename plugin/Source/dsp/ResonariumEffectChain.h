#ifndef RESONARIUMEFFECTCHAIN_H
#define RESONARIUMEFFECTCHAIN_H

#include <JuceHeader.h>

#include "Distortion.h"
#include "MultiFilter.h"
#include "MVerb.h"

class ResonariumEffectChain
{
public:
    ResonariumEffectChain(int channel, EffectChainParams params);

    void prepare(const juce::dsp::ProcessSpec& spec);

    template <typename T>
    void updateParameters(T& source, float frequency);

    void process(juce::dsp::AudioBlock<float> block) noexcept;

    int channel;

    juce::dsp::Chorus<float> chorus;
    ChorusParams chorusParams;

    gin::StereoDelay delay;
    DelayParams delayParams;

    Distortion distortion;
    DistortionParams distortionParams;

    MultiFilter filter1;
    SVFParams filter1Params;
    MultiFilter filter2;
    SVFParams filter2Params;

    juce::dsp::Phaser<float> phaser;
    PhaserParams phaserParams;

    juce::dsp::Reverb reverb;
    gin::PlateReverb<float, int> reverb2;
    MVerb<float> reverb3;
    ReverbParams reverbParams;


    juce::dsp::Gain<float> gain;
    EffectChainParams effectChainParams;

};

#endif //RESONARIUMEFFECTCHAIN_H