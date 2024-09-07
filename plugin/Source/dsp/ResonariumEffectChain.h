#ifndef RESONARIUMEFFECTCHAIN_H
#define RESONARIUMEFFECTCHAIN_H

#include <JuceHeader.h>

#include "Distortion.h"
#include "MultiDelay.h"
#include "MultiFilter.h"
#include "MVerb.h"

class ResonariumEffectChain
{
public:
    ResonariumEffectChain(ResonariumProcessor& p, int channel, EffectChainParams params);

    void prepare(const juce::dsp::ProcessSpec& spec);

    void reset();

    template <typename T>
    void updateParameters(T& source, float frequency);

    void process(juce::dsp::AudioBlock<float> block) noexcept;

    int channel;

    juce::dsp::Chorus<float> chorus;
    ChorusParams chorusParams;

    MultiDelay delay;
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
    MVerb<float> mverb;
    ReverbParams reverbParams;

    juce::dsp::Gain<float> gain;

    EffectChainParams effectChainParams;

    ResonariumProcessor& proc;

};

#endif //RESONARIUMEFFECTCHAIN_H