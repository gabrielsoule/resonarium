#include "ResonatorSynth.h"
#include "ResonatorVoice.h"

ResonatorSynth::ResonatorSynth(GlobalState& state, SynthParams params)
    : gin::Synthesiser(gin::VoicePoolConfig { .maxPolyphony = NUM_SYNTH_VOICES,
                                              .fastFadeSlots = NUM_STEAL_VOICES,
                                              .fastFadeMs = VOICE_FAST_FADE_MS }),
      state(state), params(params), effectChain(params.effectChainParams)
{
    for (auto& useState : visualSnapshotUseStates)
        useState.store(visualSnapshotSlotFree, std::memory_order_relaxed);

    state.externalInputFrame = &externalInputFrame;
    monoMSEGs.clear();
    msegData.clear();

    for (int i = 0; i < NUM_LFOS; i++)
    {
        monoLFOs[i].params = params.lfoParams[i];
    }

    for (int i = 0; i < NUM_RANDOMS; i++)
    {
        monoRandomLFOs[i].params = params.randomLfoParams[i];
    }

    for (int i = 0; i < NUM_MSEGS; i++)
    {
        auto mseg = StereoMSEGWrapper(params.msegParams[i]);
        monoMSEGs.add(mseg);
    }
}
void ResonatorSynth::prepare(const juce::dsp::ProcessSpec& spec)
{
    const bool voiceManagerPrepared = prepareVoiceManager(spec.sampleRate,
                                                          static_cast<int>(spec.maximumBlockSize),
                                                          static_cast<int>(spec.numChannels));
    jassert(voiceManagerPrepared);
    if (!voiceManagerPrepared)
        return;

    waveguideScratchArena.prepare(spec);

    externalInputFrame.prepare(static_cast<int>(spec.maximumBlockSize));
    state.sampler.prepare(spec);

    for (auto* v : voices)
    {
        dynamic_cast<ResonatorVoice*>(v)->prepare(spec);
    }

    for (auto & monoLFO : monoLFOs)
    {
        monoLFO.reset();
        monoLFO.prepare(spec);
    }

    for (auto & monoRandomLFO : monoRandomLFOs)
    {
        monoRandomLFO.reset();
        monoRandomLFO.prepare(spec);
    }

    for (int i = 0; i < NUM_MSEGS; i++)
    {
        monoMSEGs.getReference(i).prepare(spec);
    }
    effectChain.prepare(spec);
    effectChain.reset();
    currentBlockSize = 0;
    visualPublicationIntervalSamples = static_cast<std::uint64_t>(juce::jmax(
        1, juce::roundToInt(spec.sampleRate / visualSnapshotPublicationRateHz)));
    visualSamplesSincePublication = 0;
    if (visualConsumerCount.load(std::memory_order_acquire) != 0)
        visualPublicationRequested.store(true, std::memory_order_release);
    updateParameters();
    state.modMatrix.snapParams();

}

#if defined (RESONARIUM_TEST_INSTRUMENTATION)
ResonariumEffectChain::TestCallCounters ResonatorSynth::getGlobalEffectTestCallCounters() const noexcept
{
    return effectChain.getTestCallCounters();
}

void ResonatorSynth::clearGlobalEffectTestCallCounters() noexcept
{
    effectChain.clearTestCallCounters();
}

#endif

bool ResonatorSynth::beginHostBlock(int numSamples)
{
    beginVoiceHostBlock();
    currentBlockSize = numSamples;

    // Transfer the newest immutable sample owner before any MIDI assignment is
    // dispatched. Existing voices retain their latched snapshot handles.
    static_cast<void>(state.sampler.commitPendingAtBlockBoundary());

    const int rawIndex = static_cast<int>(params.soloResonator->getProcValue());
    state.soloActive = rawIndex >= 0;
    if (state.soloActive)
    {
        state.soloBankIndex = rawIndex / NUM_RESONATORS;
        state.soloResonatorIndex = rawIndex % NUM_RESONATORS;
    }

    setVoiceLimit(juce::jlimit(1, NUM_SYNTH_VOICES,
                               juce::roundToInt(params.globalParams.numVoices->getProcValue())));

    const bool shouldPanic = consumePanicRequest();
    const bool shouldReset = consumeResetRequest();
    if (shouldPanic || shouldReset)
    {
        gin::Synthesiser::panic();
        effectChain.reset();
    }

    return shouldPanic;
}

void ResonatorSynth::endBlock(int blockSize)
{
    gin::Synthesiser::endBlock(blockSize);

    // This is only a work gate; snapshot ownership is synchronised separately.
    // The sample-domain accumulator tracks ~30 Hz independently of host block
    // size, so the audio thread does not gather a full visual frame per block.
    if (visualConsumerCount.load(std::memory_order_acquire) == 0)
        return;

    if (visualPublicationRequested.exchange(false, std::memory_order_acq_rel))
    {
        visualSamplesSincePublication = publishVisualSnapshot()
                                                ? 0
                                                : visualPublicationIntervalSamples;
        return;
    }

    // A failed publication leaves the accumulator due, causing one bounded
    // retry on the next block rather than spinning on a UI-pinned slot.
    if (visualSamplesSincePublication >= visualPublicationIntervalSamples)
    {
        if (publishVisualSnapshot())
            visualSamplesSincePublication = 0;
        return;
    }

    const auto processedSamples = static_cast<std::uint64_t>(juce::jmax(0, blockSize));
    std::uint64_t remainder = 0;
    bool publicationDue = false;
    if (processedSamples >= visualPublicationIntervalSamples)
    {
        publicationDue = true;
        remainder = (visualSamplesSincePublication
                     + processedSamples % visualPublicationIntervalSamples)
                    % visualPublicationIntervalSamples;
    }
    else
    {
        const auto accumulated = visualSamplesSincePublication + processedSamples;
        publicationDue = accumulated >= visualPublicationIntervalSamples;
        remainder = publicationDue ? accumulated - visualPublicationIntervalSamples
                                   : accumulated;
    }

    if (!publicationDue)
    {
        visualSamplesSincePublication = remainder;
        return;
    }

    visualSamplesSincePublication = publishVisualSnapshot()
                                        ? remainder
                                        : visualPublicationIntervalSamples;
}

bool ResonatorSynth::publishVisualSnapshot() noexcept
{
    const auto currentSlot = publishedVisualSnapshotSlot.load(std::memory_order_acquire);
    int targetSlot = -1;
    for (size_t i = 0; i < visualSnapshots.size(); ++i)
    {
        if (static_cast<int>(i) == currentSlot)
            continue;

        auto expected = visualSnapshotSlotFree;
        if (visualSnapshotUseStates[i].compare_exchange_strong(
                expected, visualSnapshotSlotWriting,
                std::memory_order_acq_rel, std::memory_order_acquire))
        {
            targetSlot = static_cast<int>(i);
            break;
        }
    }

    // All spare slots can only be pinned when multiple editors are copying
    // concurrently. Skip this block's visual update instead of waiting.
    if (targetSlot < 0)
        return false;

    auto& frame = visualSnapshots[static_cast<size_t>(targetSlot)];

    for (int lfoIndex = 0; lfoIndex < NUM_LFOS; ++lfoIndex)
        frame.monoLFOPhases[static_cast<size_t>(lfoIndex)] = monoLFOs[lfoIndex].getCurrentPhase(0);

    for (int msegIndex = 0; msegIndex < NUM_MSEGS; ++msegIndex)
        frame.monoMSEGPhases[static_cast<size_t>(msegIndex)] =
            monoMSEGs.getReference(msegIndex).getCurrentPhase(0);

    for (int randomIndex = 0; randomIndex < NUM_RANDOMS; ++randomIndex)
        frame.monoRandomValues[static_cast<size_t>(randomIndex)] =
            monoRandomLFOs[randomIndex].centerState.currentRandomValue;

    for (int voiceIndex = 0; voiceIndex < NUM_PHYSICAL_VOICES; ++voiceIndex)
    {
        auto& published = frame.voices[static_cast<size_t>(voiceIndex)];
        published.active = false;

        auto* voice = static_cast<ResonatorVoice*>(getVoice(voiceIndex));
        if (voice == nullptr || voice->getManagerVoiceState() == gin::VoiceState::idle)
            continue;

        for (int envelopeIndex = 0; envelopeIndex < NUM_ENVELOPES; ++envelopeIndex)
        {
            const auto phase = voice->polyEnvelopes[static_cast<size_t>(envelopeIndex)].internalADSR.getCurrentPhase();
            published.envelopeStages[static_cast<size_t>(envelopeIndex)] = phase.first;
            published.envelopePhases[static_cast<size_t>(envelopeIndex)] = phase.second;
        }

        for (int lfoIndex = 0; lfoIndex < NUM_LFOS; ++lfoIndex)
            published.lfoPhases[static_cast<size_t>(lfoIndex)] =
                voice->polyLFOs[static_cast<size_t>(lfoIndex)].getCurrentPhase(0);

        for (int msegIndex = 0; msegIndex < NUM_MSEGS; ++msegIndex)
            published.msegPhases[static_cast<size_t>(msegIndex)] =
                voice->polyMSEGs.getReference(msegIndex).getCurrentPhase(0);

        published.active = true;
    }

    frame.voiceTelemetry = getVoiceTelemetry();
    frame.cpuUsage = getCpuUsage();

    visualSnapshotUseStates[static_cast<size_t>(targetSlot)].store(
        visualSnapshotSlotFree, std::memory_order_release);
    publishedVisualSnapshotSlot.store(targetSlot, std::memory_order_release);
    return true;
}

void ResonatorSynth::acquireVisualConsumer() noexcept
{
    // Publish the request before the count. An audio-thread acquire that sees
    // the new consumer therefore also sees the request for its first frame.
    visualPublicationRequested.store(true, std::memory_order_release);
    visualConsumerCount.fetch_add(1, std::memory_order_release);
}

void ResonatorSynth::releaseVisualConsumer() noexcept
{
    const auto previous = visualConsumerCount.fetch_sub(1, std::memory_order_acq_rel);
    jassert(previous != 0);
    if (previous == 0)
        visualConsumerCount.store(0, std::memory_order_release);
}

bool ResonatorSynth::copyVisualSnapshot(VisualSnapshot& destination) const noexcept
{
    const auto slot = publishedVisualSnapshotSlot.load(std::memory_order_acquire);
    auto expected = visualSnapshotSlotFree;
    if (!visualSnapshotUseStates[static_cast<size_t>(slot)].compare_exchange_strong(
            expected, visualSnapshotSlotReading,
            std::memory_order_acq_rel, std::memory_order_acquire))
    {
        return false;
    }

    destination = visualSnapshots[static_cast<size_t>(slot)];
    visualSnapshotUseStates[static_cast<size_t>(slot)].store(
        visualSnapshotSlotFree, std::memory_order_release);
    return true;
}

size_t ResonatorSynth::VisualSnapshot::copyEnvelopePhases(
    int envelopeIndex, std::span<std::pair<int, float>> destination) const noexcept
{
    if (!juce::isPositiveAndBelow(envelopeIndex, NUM_ENVELOPES) || destination.empty())
        return 0;

    size_t count = 0;
    for (const auto& voice : voices)
    {
        if (!voice.active)
            continue;

        destination[count++] = {
            voice.envelopeStages[static_cast<size_t>(envelopeIndex)],
            voice.envelopePhases[static_cast<size_t>(envelopeIndex)],
        };
        if (count == destination.size())
            break;
    }
    return count;
}

size_t ResonatorSynth::VisualSnapshot::copyLFOPhases(
    int lfoIndex, std::span<float> destination) const noexcept
{
    if (!juce::isPositiveAndBelow(lfoIndex, NUM_LFOS) || destination.empty())
        return 0;

    size_t count = 0;
    destination[count++] = monoLFOPhases[static_cast<size_t>(lfoIndex)];
    for (const auto& voice : voices)
    {
        if (count == destination.size())
            break;
        if (!voice.active)
            continue;

        destination[count++] = voice.lfoPhases[static_cast<size_t>(lfoIndex)];
    }
    return count;
}

size_t ResonatorSynth::VisualSnapshot::copyMSEGPhases(
    int msegIndex, std::span<float> destination) const noexcept
{
    if (!juce::isPositiveAndBelow(msegIndex, NUM_MSEGS) || destination.empty())
        return 0;

    size_t count = 0;
    destination[count++] = monoMSEGPhases[static_cast<size_t>(msegIndex)];
    for (const auto& voice : voices)
    {
        if (count == destination.size())
            break;
        if (!voice.active)
            continue;

        destination[count++] = voice.msegPhases[static_cast<size_t>(msegIndex)];
    }
    return count;
}

float ResonatorSynth::VisualSnapshot::getMonoRandomValue(int randomIndex) const noexcept
{
    if (!juce::isPositiveAndBelow(randomIndex, NUM_RANDOMS))
        return 0.0f;

    return monoRandomValues[static_cast<size_t>(randomIndex)];
}

void ResonatorSynth::updateParameters()
{
    for (int i = 0; i < NUM_LFOS; i++)
    {
        if (params.lfoParams[i].enabled->isOn())
        {
            float freq = 0;
            if (params.lfoParams[i].sync->getProcValue() > 0.0f)
                freq = 1.0f / gin::NoteDuration::getNoteDurations()[size_t(params.lfoParams[i].beat->getProcValue())].
                    toSeconds(state.playHead);
            else
                freq = state.modMatrix.getValue(params.lfoParams[i].rate);
            monoLFOs[i].updateParameters(state.modMatrix, freq);
            monoLFOs[i].process(currentBlockSize);
            state.modMatrix.setMonoValue(state.modSrcMonoLFO[i], monoLFOs[i].getOutput(0), 0);
            state.modMatrix.setMonoValue(state.modSrcMonoLFO[i], monoLFOs[i].getOutput(1), 1);
        }
        else
        {
            state.modMatrix.setMonoValue(state.modSrcMonoLFO[i], 0, 0);
            state.modMatrix.setMonoValue(state.modSrcMonoLFO[i], 0, 1);
        }
    }

    for (int i = 0; i < NUM_RANDOMS; i++)
    {
        if (params.randomLfoParams[i].enabled->isOn())
        {
            float rate = 0;
            if (params.randomLfoParams[i].sync->getProcValue() > 0.0f)
                rate = 1.0f / gin::NoteDuration::getNoteDurations()[size_t(
                        params.randomLfoParams[i].beat->getProcValue())].
                    toSeconds(state.playHead);
            else
                rate = state.modMatrix.getValue(params.randomLfoParams[i].rate);
            monoRandomLFOs[i].updateParametersMono(state.modMatrix, rate);
            monoRandomLFOs[i].process(currentBlockSize);
            state.modMatrix.setMonoValue(state.modSrcMonoRND[i], monoRandomLFOs[i].getOutput(0), 0);
            state.modMatrix.setMonoValue(state.modSrcMonoRND[i], monoRandomLFOs[i].getOutput(1), 1);
        }
    }

    for (int i = 0; i < NUM_MSEGS; i++)
    {
        if (params.msegParams[i].enabled->isOn())
        {
            float rate = 0;
            if (params.msegParams[i].sync->getProcValue() > 0.0f)
                rate = 1.0f / gin::NoteDuration::getNoteDurations()[size_t(params.msegParams[i].beat->getProcValue())].
                    toSeconds(state.playHead);
            else
                rate = state.modMatrix.getValue(params.msegParams[i].rate);
            monoMSEGs.getReference(i).updateParameters(state.modMatrix, rate);
            monoMSEGs.getReference(i).process(currentBlockSize);
            state.modMatrix.setMonoValue(state.modSrcMonoMSEG[i], monoMSEGs.getReference(i).getOutput(0), 0);
            state.modMatrix.setMonoValue(state.modSrcMonoMSEG[i], monoMSEGs.getReference(i).getOutput(1), 1);
        }
        else
        {
            state.modMatrix.setMonoValue(state.modSrcMonoMSEG[i], 0, 0);
            state.modMatrix.setMonoValue(state.modSrcMonoMSEG[i], 0, 1);
        }
    }

    for(int i = 0; i < NUM_MACROS; i++)
    {
        state.modMatrix.setMonoValue(state.modSrcMacro[i], state.modMatrix.getValue(params.macroParams[i]), 0);
        state.modMatrix.setMonoValue(state.modSrcMacro[i], state.modMatrix.getValue(params.macroParams[i]), 1);
    }

    effectChain.updateParameters(state.modMatrix, state.playHead);
}

void ResonatorSynth::renderNextSubBlock(juce::AudioBuffer<float>& outputAudio, int startSample, int numSamples)
{
    currentBlockSize = numSamples;
    updateParameters();
    Synthesiser::renderNextSubBlock(outputAudio, startSample, numSamples);
    juce::dsp::AudioBlock<float> block = juce::dsp::AudioBlock<float>(outputAudio).getSubBlock(startSample, numSamples);

    effectChain.process(block);
}

void ResonatorSynth::handleController(int, int controller, int)
{
    // Gin hard-retires all voices for CC120 immediately after this callback.
    // Clear the one global post-mix chain at the same MIDI event boundary so
    // All Sound Off cannot leave delay or reverb tails audible.
    if (controller == 120)
        effectChain.reset();
}

void ResonatorSynth::panic()
{
    panicRequested.store(true, std::memory_order_release);
}
