#include "ResonariumEffectChain.h"

#include "../defines.h"

ResonariumEffectChain::ResonariumEffectChain(EffectChainParams params)
    : chorusParams(params.chorusParams),
      delay(MAX_DELAY_IN_SECONDS),
      delayParams(params.delayParams),
      distortion(params.distortionParams),
      distortionParams(params.distortionParams),
      multiAmp([this]() -> double { return sampleRate; }, params.multiAmpParams),
      multiAmpParams(params.multiAmpParams),
      phaserParams(params.phaserParams),
      compressorParams(params.compressorParams),
      reverbParams(params.reverbParams),
      filter1(params.filterParams[0]),
      filter2(params.filterParams[1]),
      filter1Params(params.filterParams[0]),
      filter2Params(params.filterParams[1])
{
}

void ResonariumEffectChain::prepare(const juce::dsp::ProcessSpec& spec)
{
    this->sampleRate = spec.sampleRate;
    chorus.prepare(spec);
    delay.prepare(spec);
    distortion.prepare(spec);
    filter1.prepare(spec);
    filter2.prepare(spec);
    phaser.prepare(spec);
    compressor.prepare(spec);
    mverb.setSampleRate(static_cast<float>(spec.sampleRate));
    enabledEffects = readEnabledEffects();
    invalidateControlCaches();
}

void ResonariumEffectChain::reset()
{
   #if defined (RESONARIUM_TEST_INSTRUMENTATION)
    ++testCallCounters.resets;
   #endif

    enabledEffects = readEnabledEffects();

    if (enabledEffects.chorus) chorus.reset();
    if (enabledEffects.delay) delay.reset();
    if (enabledEffects.filter1) filter1.reset();
    if (enabledEffects.filter2) filter2.reset();
    if (enabledEffects.phaser) phaser.reset();
    if (enabledEffects.reverb) mverb.reset();
    if (enabledEffects.distortion) distortion.reset();
    if (enabledEffects.multiAmp) multiAmp.reset();
    if (enabledEffects.compressor) compressor.reset();
    invalidateControlCaches();
}

ResonariumEffectChain::EnabledEffects ResonariumEffectChain::readEnabledEffects() const noexcept
{
    return {
        chorusParams.enabled->isOn(),
        delayParams.enabled->isOn(),
        distortionParams.enabled->isOn(),
        multiAmpParams.enabled->isOn(),
        phaserParams.enabled->isOn(),
        compressorParams.enabled->isOn(),
        reverbParams.enabled->isOn(),
        filter1Params.enabled->isOn(),
        filter2Params.enabled->isOn(),
    };
}

void ResonariumEffectChain::resetNewlyEnabled(const EnabledEffects& nextState)
{
    bool resetAny = false;
    if (nextState.chorus && !enabledEffects.chorus) { chorus.reset(); resetAny = true; }
    if (nextState.delay && !enabledEffects.delay) { delay.reset(); resetAny = true; }
    if (nextState.distortion && !enabledEffects.distortion) { distortion.reset(); resetAny = true; }
    if (nextState.multiAmp && !enabledEffects.multiAmp) { multiAmp.reset(); resetAny = true; }
    if (nextState.phaser && !enabledEffects.phaser) { phaser.reset(); resetAny = true; }
    if (nextState.compressor && !enabledEffects.compressor) { compressor.reset(); resetAny = true; }
    if (nextState.reverb && !enabledEffects.reverb) { mverb.reset(); resetAny = true; }
    if (nextState.filter1 && !enabledEffects.filter1) { filter1.reset(); resetAny = true; }
    if (nextState.filter2 && !enabledEffects.filter2) { filter2.reset(); resetAny = true; }

    if (resetAny)
        invalidateControlCaches();
}

void ResonariumEffectChain::invalidateControlCaches() noexcept
{
    const auto invalidate = [] (auto& controls)
    {
        for (auto& control : controls)
            control.invalidate();
    };

    invalidate(chorusControls);
    invalidate(phaserControls);
    invalidate(reverbControls);
    invalidate(delayControls);
    delayPingPong.invalidate();
    invalidate(compressorControls);
}

template <typename T>
void ResonariumEffectChain::updateParameters(T& source, juce::AudioPlayHead* playhead)
{
   #if defined (RESONARIUM_TEST_INSTRUMENTATION)
    ++testCallCounters.parameterUpdates;
   #endif

    const auto nextEnabledEffects = readEnabledEffects();
    resetNewlyEnabled(nextEnabledEffects);
    enabledEffects = nextEnabledEffects;

    if (enabledEffects.chorus)
    {
        const std::array<float, 5> values {
            source.getValue(chorusParams.rate, 0),
            source.getValue(chorusParams.depth, 0),
            source.getValue(chorusParams.feedback, 0),
            source.getValue(chorusParams.delay, 0),
            source.getValue(chorusParams.mix, 0),
        };
        if (chorusControls[0].update(values[0])) chorus.setRate(values[0]);
        if (chorusControls[1].update(values[1])) chorus.setDepth(values[1]);
        if (chorusControls[2].update(values[2])) chorus.setFeedback(values[2]);
        if (chorusControls[3].update(values[3])) chorus.setCentreDelay(values[3]);
        if (chorusControls[4].update(values[4])) chorus.setMix(values[4]);
    }

    if (enabledEffects.phaser)
    {
        const std::array<float, 5> values {
            source.getValue(phaserParams.rate, 0),
            source.getValue(phaserParams.depth, 0),
            source.getValue(phaserParams.feedback, 0),
            source.getValue(phaserParams.centreFrequency, 0),
            source.getValue(phaserParams.mix, 0),
        };
        if (phaserControls[0].update(values[0])) phaser.setRate(values[0]);
        if (phaserControls[1].update(values[1])) phaser.setDepth(values[1]);
        if (phaserControls[2].update(values[2])) phaser.setFeedback(values[2]);
        if (phaserControls[3].update(values[3])) phaser.setCentreFrequency(values[3]);
        if (phaserControls[4].update(values[4])) phaser.setMix(values[4]);
    }

    if (enabledEffects.reverb)
    {
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

        const std::array<float, 8> values {
            mverbDampingFreq, mverbDensity, mverbBandwidthFreq, mverbDecay,
            mverbPredelay, mverbRoomSize, mverbMix, mverbEarlyMix,
        };
        if (reverbControls[0].update(values[0])) mverb.setParameter(MVerb<float>::DAMPINGFREQ, values[0]);
        if (reverbControls[1].update(values[1])) mverb.setParameter(MVerb<float>::DENSITY, values[1]);
        if (reverbControls[2].update(values[2])) mverb.setParameter(MVerb<float>::BANDWIDTHFREQ, values[2]);
        if (reverbControls[3].update(values[3])) mverb.setParameter(MVerb<float>::DECAY, values[3]);
        if (reverbControls[4].update(values[4])) mverb.setParameter(MVerb<float>::PREDELAY, values[4]);
        if (reverbControls[5].update(values[5])) mverb.setParameter(MVerb<float>::SIZE, values[5]);
        if (reverbControls[6].update(values[6])) mverb.setParameter(MVerb<float>::MIX, values[6]);
        if (reverbControls[7].update(values[7])) mverb.setParameter(MVerb<float>::EARLYMIX, values[7]);
    }

    if (enabledEffects.delay)
    {
        float delayTimeL = 0;
        float delayTimeR = 0;
        if (delayParams.syncL->getProcValue() > 0.0f)
        {
            delayTimeL = gin::NoteDuration::getNoteDurations()[size_t(delayParams.beatL->getProcValue())].
                toSeconds(playhead);
        }
        else
        {
            delayTimeL = source.getValue(delayParams.timeL, 0);
        }

        if (delayControls[0].update(delayTimeL)) delay.setDelayTime(0, delayTimeL);

        if (delayParams.lock->isOn())
        {
            delayTimeR = delayTimeL;
        }
        else if (delayParams.syncR->getProcValue() > 0.0f)
        {
            delayTimeR = gin::NoteDuration::getNoteDurations()[size_t(delayParams.beatR->getProcValue())].
                toSeconds(playhead);
        }
        else
        {
            delayTimeR = source.getValue(delayParams.timeR, 1);
        }

        const auto feedbackL = source.getValue(delayParams.feedback, 0);
        const auto feedbackR = source.getValue(delayParams.feedback, 1);
        const auto mixL = source.getValue(delayParams.mix, 0);
        const auto mixR = source.getValue(delayParams.mix, 1);
        const auto pingPong = delayParams.pingPongAmount->isOn();
        if (delayControls[1].update(delayTimeR)) delay.setDelayTime(1, delayTimeR);
        if (delayControls[2].update(feedbackL)) delay.setFeedback(0, feedbackL);
        if (delayControls[3].update(feedbackR)) delay.setFeedback(1, feedbackR);
        if (delayPingPong.update(pingPong)) delay.setPingPong(pingPong);
        if (delayControls[4].update(mixL)) delay.setMix(0, mixL);
        if (delayControls[5].update(mixR)) delay.setMix(1, mixR);
    }

    if (enabledEffects.compressor)
    {
        const std::array<float, 4> values {
            source.getValue(compressorParams.threshold),
            source.getValue(compressorParams.ratio),
            source.getValue(compressorParams.attack),
            source.getValue(compressorParams.release),
        };
        if (compressorControls[0].update(values[0])) compressor.setThreshold(values[0]);
        if (compressorControls[1].update(values[1])) compressor.setRatio(values[1]);
        if (compressorControls[2].update(values[2])) compressor.setAttack(values[2]);
        if (compressorControls[3].update(values[3])) compressor.setRelease(values[3]);
    }

    if (enabledEffects.distortion) distortion.updateParameters(source);
    if (enabledEffects.multiAmp) multiAmp.updateParameters(source);
    if (enabledEffects.filter1) filter1.updateParameters(source);
    if (enabledEffects.filter2) filter2.updateParameters(source);

}

void ResonariumEffectChain::process(juce::dsp::AudioBlock<float> block) noexcept
{
   #if defined (RESONARIUM_TEST_INSTRUMENTATION)
    ++testCallCounters.processCalls;
   #endif

    if (!enabledEffects.any())
        return;

    const juce::dsp::ProcessContextReplacing context(block);
    if (enabledEffects.filter1) filter1.process(context);
    if (enabledEffects.chorus) chorus.process(context);
    if (enabledEffects.phaser) phaser.process(context);
    if (enabledEffects.distortion) distortion.process(context);
    if (enabledEffects.multiAmp) multiAmp.process(context);
    if (enabledEffects.delay) delay.process(context);
    if (enabledEffects.compressor) compressor.process(context);
    if (enabledEffects.reverb)
    {
        float* data[2] = {block.getChannelPointer(0), block.getChannelPointer(1)};
        mverb.process(data, data, block.getNumSamples());
    }
    if (enabledEffects.filter2) filter2.process(context);
}


template void ResonariumEffectChain::updateParameters<gin::ModMatrix>(gin::ModMatrix&, juce::AudioPlayHead*);
