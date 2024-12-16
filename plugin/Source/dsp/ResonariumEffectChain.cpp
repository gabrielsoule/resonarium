#include "ResonariumEffectChain.h"

#include "../ResonatorVoice.h"
#include "../PluginProcessor.h"
#include "../defines.h"

ResonariumEffectChain::ResonariumEffectChain(ResonariumProcessor& p, int channel, EffectChainParams params)
    : chorusParams(params.chorusParams),
      delay(p, MAX_DELAY_IN_SECONDS),
      delayParams(params.delayParams),
      distortion(p, params.distortionParams),
      distortionParams(params.distortionParams),
      multiAmp(p, params.multiAmpParams),
      multiAmpParams(params.multiAmpParams),
      phaserParams(params.phaserParams),
      compressorParams(params.compressorParams),
      reverbParams(params.reverbParams),
      filter1(params.filterParams[0]),
      filter2(params.filterParams[1]),
      filter1Params(params.filterParams[0]),
      filter2Params(params.filterParams[1]),
      effectChainParams(params),
      proc(p)
{
    this->channel = channel;
}

void ResonariumEffectChain::prepare(const juce::dsp::ProcessSpec& spec)
{
    chorus.prepare(spec);
    delay.prepare(spec);
    distortion.prepare(spec);
    filter1.prepare(spec);
    filter2.prepare(spec);
    phaser.prepare(spec);
    gain.prepare(spec);
    filter1.prepare(spec);
    filter2.prepare(spec);
}

void ResonariumEffectChain::reset()
{
    chorus.reset();
    delay.reset();
    filter1.reset();
    filter2.reset();
    phaser.reset();
    mverb.reset();
    gain.reset();
    distortion.reset();
    multiAmp.reset();
    filter1.reset();
    filter2.reset();
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

    const float mverbDampingFreq = source.getValue(reverbParams.dampingFreq);
    const float mverbDensity = source.getValue(reverbParams.density);
    const float mverbBandwidthFreq = source.getValue(reverbParams.bandwidthFreq);
    const float mverbDecay = source.getValue(reverbParams.decay);
    const float mverbPredelay = source.getValue(reverbParams.predelay);
    const float mverbRoomSize = source.getValue(reverbParams.size);
    const float mverbMix = source.getValue(reverbParams.mix);
    const float mverbEarlyMix = source.getValue(reverbParams.earlyMix);

    jassert(mverbDampingFreq >= 0 && mverbDampingFreq <= 1);
    jassert(mverbDensity >= 0 && mverbDensity <= 1);
    jassert(mverbBandwidthFreq >= 0 && mverbBandwidthFreq <= 1);
    jassert(mverbDecay >= 0 && mverbDecay <= 1);
    jassert(mverbPredelay >= 0 && mverbPredelay <= 1);
    jassert(mverbRoomSize >= 0 && mverbRoomSize <= 1);
    jassert(mverbMix >= 0 && mverbMix <= 1);
    jassert(mverbEarlyMix >= 0 && mverbEarlyMix <= 1);

    mverb.setParameter(MVerb<float>::DAMPINGFREQ, mverbDampingFreq);
    mverb.setParameter(MVerb<float>::DENSITY, mverbDensity);
    mverb.setParameter(MVerb<float>::BANDWIDTHFREQ, mverbBandwidthFreq);
    mverb.setParameter(MVerb<float>::DECAY, mverbDecay);
    mverb.setParameter(MVerb<float>::PREDELAY, mverbPredelay);
    mverb.setParameter(MVerb<float>::SIZE, mverbRoomSize);
    mverb.setParameter(MVerb<float>::GAIN, 1);
    mverb.setParameter(MVerb<float>::MIX, mverbMix);
    mverb.setParameter(MVerb<float>::EARLYMIX, mverbEarlyMix);

    float delayTimeL = 0;
    float delayTimeR = 0;
    if (delayParams.syncL->getProcValue() > 0.0f)
    {
        delayTimeL = gin::NoteDuration::getNoteDurations()[size_t(delayParams.beatL->getProcValue())].
            toSeconds(proc.getPlayHead());
    }
    else
    {
        delayTimeL = source.getValue(delayParams.timeL, 0);
    }

    delay.setDelayTime(0, delayTimeL);

    if (delayParams.lock->isOn())
    {
        delayTimeR = delayTimeL;
    }
    else
    {
        if (delayParams.syncR->getProcValue() > 0.0f)
        {
            delayTimeR = gin::NoteDuration::getNoteDurations()[size_t(delayParams.beatR->getProcValue())].
                toSeconds(proc.getPlayHead());
        }
        else
        {
            delayTimeR = source.getValue(delayParams.timeR, 1);
        }
    }

    delay.setDelayTime(1, delayTimeR);
    delay.setFeedback(0, source.getValue(delayParams.feedback, 0));
    delay.setFeedback(1, source.getValue(delayParams.feedback, 1));
    delay.setPingPongAmount(0, source.getValue(delayParams.pingPongAmount, 0));
    delay.setPingPongAmount(1, source.getValue(delayParams.pingPongAmount, 1));
    delay.setMix(0, source.getValue(delayParams.mix, 0));
    delay.setMix(1, source.getValue(delayParams.mix, 1));

    compressor.setThreshold(source.getValue(compressorParams.threshold));
    compressor.setRatio(source.getValue(compressorParams.ratio));
    compressor.setAttack(source.getValue(compressorParams.attack));
    compressor.setRelease(source.getValue(compressorParams.release));


    distortion.updateParameters(source);

    multiAmp.updateParameters(source);

    filter1.updateParameters(source);

    filter2.updateParameters(source);
}

void ResonariumEffectChain::process(juce::dsp::AudioBlock<float> block) noexcept
{
    const juce::dsp::ProcessContextReplacing context(block);
    if (filter1Params.enabled->isOn()) filter1.process(context);
    if (chorusParams.enabled->isOn()) chorus.process(context);
    if (phaserParams.enabled->isOn()) phaser.process(context);
    if (distortionParams.enabled->isOn()) distortion.process(context);
    if (multiAmpParams.enabled->isOn()) multiAmp.process(context);
    if (delayParams.enabled->isOn()) delay.process(context);
    if (compressorParams.enabled->isOn()) compressor.process(context);
    if (reverbParams.enabled->isOn())
    {
        float* data[2] = {block.getChannelPointer(0), block.getChannelPointer(1)};
        mverb.process(data, data, block.getNumSamples());
    }
    if (filter2Params.enabled->isOn()) filter2.process(context);
}


template void ResonariumEffectChain::updateParameters<gin::ModVoice>(gin::ModVoice&, float);
template void ResonariumEffectChain::updateParameters<gin::ModMatrix>(gin::ModMatrix&, float);
template void ResonariumEffectChain::updateParameters<ResonatorVoice>(ResonatorVoice&, float);
