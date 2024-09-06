#include "ResonariumEffectChain.h"

#include "../ResonatorVoice.h"

ResonariumEffectChain::ResonariumEffectChain(int channel, EffectChainParams params)
    : chorusParams(params.chorusParams),
      delayParams(params.delayParams),
      distortionParams(params.distortionParams),
      filter1Params(params.filterParams[0]),
      filter2Params(params.filterParams[1]),
      phaserParams(params.phaserParams),
      reverbParams(params.reverbParams),
      effectChainParams(params)
{
    this->channel = channel;
}

void ResonariumEffectChain::prepare(const juce::dsp::ProcessSpec& spec)
{
    chorus.prepare(spec);
    delay.setSampleRate(spec.sampleRate);
    // distortion.prepare(spec);
    filter1.prepare(spec);
    filter2.prepare(spec);
    phaser.prepare(spec);
    reverb.prepare(spec);
    gain.prepare(spec);
}

void ResonariumEffectChain::reset()
{
    chorus.reset();
    delay.reset();
    filter1.reset();
    filter2.reset();
    phaser.reset();
    reverb.reset();
    mverb.reset();
    gain.reset();
}

template <typename T>
void ResonariumEffectChain::updateParameters(T& source, float frequency)
{
    chorus.setRate(source.getValue(chorusParams.rate, channel));
    chorus.setDepth(source.getValue(chorusParams.depth, channel));
    chorus.setFeedback(source.getValue(chorusParams.feedback, channel));
    chorus.setCentreDelay(source.getValue(chorusParams.delay, channel));
    chorus.setMix(source.getValue(chorusParams.mix, channel));

    phaser.setRate(source.getValue(phaserParams.rate, channel));
    phaser.setDepth(source.getValue(phaserParams.depth, channel));
    phaser.setFeedback(source.getValue(phaserParams.feedback, channel));
    phaser.setCentreFrequency(source.getValue(phaserParams.centreFrequency, channel));
    phaser.setMix(source.getValue(phaserParams.mix, channel));

    // float reverbMix = source.getValue(reverbParams.mix, channel);
    // reverb.setParameters({
    //     source.getValue(reverbParams.roomSize, channel),
    //     source.getValue(reverbParams.damping, channel),
    //     reverbMix,
    //     1 - reverbMix,
    //     source.getValue(reverbParams.width, channel)
    // });

    // reverb2.setDamping(juce::jmap(source.getValue(reverbParams.damping), 0.0f, 20000.0f));
    // reverb2.setDecay(0);
    // reverb2.setSize(source.getValue(reverbParams.roomSize));
    // reverb2.setLowpass(0);
    // reverb2.setPredelay(0);
    // reverb2.setMix(reverbMix);

    const float mverbDampingFreq = source.getValue(reverbParams.dampingFreq);
    const float mverbDensity = source.getValue(reverbParams.density);
    const float mverbBandwidthFreq = source.getValue(reverbParams.bandwidthFreq);
    const float mverbDecay = source.getValue(reverbParams.decay);
    const float mverbPredelay = source.getValue(reverbParams.predelay);
    const float mverbRoomSize = source.getValue(reverbParams.size);
    const float mverbMix = source.getValue(reverbParams.mix);
    const float mverbEarlyMix = source.getValue(reverbParams.earlyMix);

    //jassert between 0 and 1
    jassert(mverbDampingFreq >= 0 && mverbDampingFreq <= 1);
    jassert(mverbDensity >= 0 && mverbDensity <= 1);
    jassert(mverbBandwidthFreq >= 0 && mverbBandwidthFreq <= 1);
    jassert(mverbDecay >= 0 && mverbDecay <= 1);
    jassert(mverbPredelay >= 0 && mverbPredelay <= 1);
    jassert(mverbRoomSize >= 0 && mverbRoomSize <= 1);
    jassert(mverbMix >= 0 && mverbMix <= 1);
    jassert(mverbEarlyMix >= 0 && mverbEarlyMix <= 1);

    mverb.setParameter (MVerb<float>::DAMPINGFREQ, mverbDampingFreq);
    mverb.setParameter (MVerb<float>::DENSITY, mverbDensity);
    mverb.setParameter (MVerb<float>::BANDWIDTHFREQ, mverbBandwidthFreq);
    mverb.setParameter (MVerb<float>::DECAY, mverbDecay);
    mverb.setParameter (MVerb<float>::PREDELAY, mverbPredelay);
    mverb.setParameter (MVerb<float>::SIZE, mverbRoomSize);
    mverb.setParameter (MVerb<float>::GAIN, 1);
    mverb.setParameter (MVerb<float>::MIX, mverbMix);
    mverb.setParameter (MVerb<float>::EARLYMIX, mverbEarlyMix);
}

void ResonariumEffectChain::process(juce::dsp::AudioBlock<float> block) noexcept
{
    const juce::dsp::ProcessContextReplacing context(block);
    if(chorusParams.enabled->isOn()) chorus.process(context);
    if(phaserParams.enabled->isOn()) phaser.process(context);
    if(reverbParams.enabled->isOn())
    {
        float* data[2] = { block.getChannelPointer(0), block.getChannelPointer(1) };
        mverb.process (data, data, block.getNumSamples());
    }
}


template void ResonariumEffectChain::updateParameters<gin::ModVoice>(gin::ModVoice&, float);
template void ResonariumEffectChain::updateParameters<gin::ModMatrix>(gin::ModMatrix&, float);
template void ResonariumEffectChain::updateParameters<ResonatorVoice>(ResonatorVoice&, float);

// Explicit template instantiation for any types you'll use with updateParameters