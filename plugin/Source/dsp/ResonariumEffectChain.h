#ifndef RESONARIUMEFFECTCHAIN_H
#define RESONARIUMEFFECTCHAIN_H

#include <JuceHeader.h>

#include "Distortion.h"
#include "MultiAmp.h"
#include "MultiDelay.h"
#include "MultiFilter.h"
#include "MVerb.h"
#include "WrappedSVF.h"

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

    MultiAmp multiAmp;
    MultiAmpParams multiAmpParams;

    juce::dsp::Phaser<float> phaser;
    PhaserParams phaserParams;

    juce::dsp::Compressor<float> compressor;
    CompressorParams compressorParams;

    MVerb<float> mverb;
    ReverbParams reverbParams;

    WrappedSVF filter1;
    WrappedSVF filter2;
    SVFParams filter1Params;
    SVFParams filter2Params;


    juce::dsp::Gain<float> gain;

    EffectChainParams effectChainParams;

    ResonariumProcessor& proc;

};

#endif //RESONARIUMEFFECTCHAIN_H