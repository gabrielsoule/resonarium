#include "ResonatorVoice.h"

#include <utility>

ResonatorVoice::ResonatorVoice(GlobalState& state, VoiceParams params,
                               WaveguideScratchArena& scratchArena)
    : state(state), params(params), waveguideScratchArena(scratchArena),
      exciterRack(state, *this, params)
{
    frequency = 440.0f;
    this->disableSmoothing = true;

    polyMSEGs.clear();

    juce::dsp::IIR::Coefficients<float>::Ptr dcBlockerCoefficients =
        new juce::dsp::IIR::Coefficients<float>(1, -1, 1, -0.995f);

    for (int i = 0; i < NUM_RESONATOR_BANKS; i++)
    {
        auto* waveguideBank =
            new WaveguideResonatorBank(state, *this, params.waveguideResonatorBankParams[i]);
        resonatorBanks.add(waveguideBank);
        dcBlockers[i].state = dcBlockerCoefficients;
    }

    for (int i = 0; i < NUM_LFOS; i++)
    {
        polyLFOs[i].params = params.lfoParams[i];
    }

    for (int i = 0; i < NUM_RANDOMS; i++)
    {
        polyRandomLFOs[i].params = params.randomLfoParams[i];
    }

    for (int i = 0; i < NUM_ENVELOPES; i++)
    {
        polyEnvelopes[i].params = params.adsrParams[i];
    }

    for (int i = 0; i < NUM_MSEGS; i++)
    {
        auto mseg = StereoMSEGWrapper(params.msegParams[i]);
        polyMSEGs.add(mseg);
    }

    skewedFrequencyRange = {MIN_FILTER_FREQUENCY, MAX_FILTER_FREQUENCY, 0.0f, FREQUENCY_KNOB_SKEW};
}

ResonatorVoice::~ResonatorVoice()
{
    resonatorBanks.clear(true);
}

void ResonatorVoice::prepare(const juce::dsp::ProcessSpec& spec)
{
    MPESynthesiserVoice::setCurrentSampleRate(spec.sampleRate);
    noteSmoother.setSampleRate(spec.sampleRate);
    
    // Use setSize instead of creating new buffers - more efficient when 
    // prepare() is called multiple times with the same specs
    exciterBuffer.setSize(spec.numChannels, spec.maximumBlockSize, false, true, true);
    tempBuffer.setSize(spec.numChannels, spec.maximumBlockSize, false, true, true);
    soloBuffer.setSize(spec.numChannels, spec.maximumBlockSize, false, true, true);
    
    // Clear all buffers
    exciterBuffer.clear();
    tempBuffer.clear();
    soloBuffer.clear();

    exciterRack.prepare(spec);

    for (auto* resonatorBank : resonatorBanks)
    {
        resonatorBank->prepare(spec);
    }

    for (auto& dcBlocker : dcBlockers)
    {
        dcBlocker.prepare(spec);
    }

    for (auto& l : polyLFOs)
    {
        l.prepare(spec);
        l.reset();
    }

    for (auto& r : polyRandomLFOs)
    {
        r.prepare(spec);
        r.voice = this;
        r.reset();
    }

    for (auto& e : polyEnvelopes)
    {
        e.prepare(spec);
        e.reset();
    }

    for (auto& m : polyMSEGs)
    {
        m.reset();
        m.prepare(spec);
    }

}

void ResonatorVoice::noteStarted()
{
    startVoice();
    auto note = getCurrentlyPlayingNote();
    if (glideInfo.fromNote >= 0 && (glideInfo.glissando || glideInfo.portamento))
    {
        noteSmoother.setValueUnsmoothed(glideInfo.fromNote / 127.0f);
        noteSmoother.setTime(juce::jmax(0.001, static_cast<double>(glideInfo.rate) * 0.001));
        noteSmoother.setValue(note.initialNote / 127.0f);
    }
    else
    {
        noteSmoother.setValueUnsmoothed(note.initialNote / 127.0f);
    }

    state.modMatrix.setPolyValue(*this, state.modSrcVelocity, note.noteOnVelocity.asUnsignedFloat(), 0);
    state.modMatrix.setPolyValue(*this, state.modSrcVelocity, note.noteOnVelocity.asUnsignedFloat(), 1);
    state.modMatrix.setPolyValue(*this, state.modSrcTimbre, note.initialTimbre.asUnsignedFloat(), 0);
    state.modMatrix.setPolyValue(*this, state.modSrcTimbre, note.initialTimbre.asUnsignedFloat(), 1);
    state.modMatrix.setPolyValue(*this, state.modSrcPressure, note.pressure.asUnsignedFloat(), 0);
    state.modMatrix.setPolyValue(*this, state.modSrcPressure, note.pressure.asUnsignedFloat(), 1);

    noteReleased = false;
    quietTailSamples = 0;
    numBlocksSinceNoteOn = 0;

    // Invalidate the old assignment before the zero-sample control commit.
    // The new note's initial delay then snaps immediately, while its first
    // actual glide or bend is treated as a retune and crossfaded.
    for (auto* resonatorBank : resonatorBanks)
        resonatorBank->reset();

    // A sample snapshot and deterministic random identity belong to the full
    // assignment. Legato retargets intentionally do not pass this point.
    exciterRack.beginAssignment(id, getVoiceGeneration());
    updateParameters(0);

    for (auto& lfo : polyLFOs)
    {
        lfo.reset();
    }

    for (int i = 0; i < NUM_LFOS; ++i)
    {
        const auto phase = params.lfoParams[i].retrig->getBoolValue()
                               ? 0.0f
                               : noteRandom.nextFloat() * static_cast<float>(gin::LFO::maxRandomPhase);
        polyLFOs[i].noteOn(phase);
    }

    for (auto& envelope : polyEnvelopes)
    {
        envelope.reset();
        envelope.noteOn();
    }

    for (auto& mseg : polyMSEGs)
    {
        mseg.reset();
        mseg.noteOn();
    }

    for (auto& dcBlocker : dcBlockers)
    {
        dcBlocker.reset();
    }

}

void ResonatorVoice::noteRetriggered()
{
    const auto note = getCurrentlyPlayingNote();
    if (glideInfo.fromNote >= 0 && (glideInfo.glissando || glideInfo.portamento))
    {
        noteSmoother.setTime(juce::jmax(0.001, static_cast<double>(glideInfo.rate) * 0.001));
        noteSmoother.setValue(note.initialNote / 127.0f);
    }
    else
    {
        noteSmoother.setValueUnsmoothed(note.initialNote / 127.0f);
    }

    state.modMatrix.setPolyValue(*this, state.modSrcVelocity, note.noteOnVelocity.asUnsignedFloat(), 0);
    state.modMatrix.setPolyValue(*this, state.modSrcVelocity, note.noteOnVelocity.asUnsignedFloat(), 1);
    state.modMatrix.setPolyValue(*this, state.modSrcTimbre, note.initialTimbre.asUnsignedFloat(), 0);
    state.modMatrix.setPolyValue(*this, state.modSrcTimbre, note.initialTimbre.asUnsignedFloat(), 1);
    state.modMatrix.setPolyValue(*this, state.modSrcPressure, note.pressure.asUnsignedFloat(), 0);
    state.modMatrix.setPolyValue(*this, state.modSrcPressure, note.pressure.asUnsignedFloat(), 1);
}

void ResonatorVoice::noteStopped(bool allowTailOff)
{
    noteReleased = true;

    exciterRack.noteStopped(allowTailOff);

    for (int i = 0; i < NUM_ENVELOPES; i++)
    {
        polyEnvelopes[i].noteOff();
    }
}

bool ResonatorVoice::isVoiceActive()
{
    return getManagerVoiceState() != gin::VoiceState::idle;
}

void ResonatorVoice::updateParameters(int numSamples)
{
    auto note = getCurrentlyPlayingNote();
    state.modMatrix.setPolyValue(*this, state.modSrcNote, note.initialNote / 127.0f);
    noteSmoother.process(numSamples);
    currentMidiNote = noteSmoother.getCurrentValue() * 127.0f;
    currentMidiNote += static_cast<float>(note.totalPitchbendInSemitones);
    frequency = gin::getMidiNoteInHertz(currentMidiNote);
    const float normalizedFrequency = skewedFrequencyRange.convertTo0to1(frequency);
    state.modMatrix.setPolyValue(*this, state.modSrcFrequency, normalizedFrequency);

    for (int i = 0; i < NUM_LFOS; i++)
    {
        if (params.lfoParams[i].enabled->isOn())
        {
            float freq = 0;
            if (params.lfoParams[i].sync->getProcValue() > 0.0f)
                freq = 1.0f / gin::NoteDuration::getNoteDurations()[size_t(params.lfoParams[i].beat->getProcValue())].
                    toSeconds(state.playHead);
            else
                freq = getValue(params.lfoParams[i].rate);
            polyLFOs[i].updateParameters(*this, freq);
            polyLFOs[i].process(numSamples);
            // TODO: avoid the duplicate parameter lookup.
            state.modMatrix.setPolyValue(*this, state.modSrcPolyLFO[i], polyLFOs[i].getOutput(0), 0);
            state.modMatrix.setPolyValue(*this, state.modSrcPolyLFO[i], polyLFOs[i].getOutput(1), 1);
        }
        else
        {
            state.modMatrix.setPolyValue(*this, state.modSrcPolyLFO[i], polyLFOs[i].getOutput(0), 0);
            state.modMatrix.setPolyValue(*this, state.modSrcPolyLFO[i], polyLFOs[i].getOutput(1), 1);
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
                rate = getValue(params.randomLfoParams[i].rate);
            polyRandomLFOs[i].updateParameters(rate);
            polyRandomLFOs[i].process(numSamples);
            state.modMatrix.setPolyValue(*this, state.modSrcPolyRND[i], polyRandomLFOs[i].getOutput(0), 0);
            state.modMatrix.setPolyValue(*this, state.modSrcPolyRND[i], polyRandomLFOs[i].getOutput(1), 1);
        }
    }

    for (int i = 0; i < NUM_ENVELOPES; i++)
    {
        polyEnvelopes[i].updateParameters(*this);
        polyEnvelopes[i].process(numSamples);
        state.modMatrix.setPolyValue(*this, state.modSrcPolyENV[i], polyEnvelopes[i].getOutput(), 0);
        state.modMatrix.setPolyValue(*this, state.modSrcPolyENV[i], polyEnvelopes[i].getOutput(), 1);
    }

    for (int i = 0; i < NUM_MSEGS; i++)
    {
        if (params.msegParams[i].enabled->isOn())
        {
            float freq = 0;
            if (params.msegParams[i].sync->getProcValue() > 0.0f)
                freq = 1.0f / gin::NoteDuration::getNoteDurations()[size_t(params.msegParams[i].beat->getProcValue())].
                    toSeconds(state.playHead);
            else
                freq = getValue(params.msegParams[i].rate);
            polyMSEGs.getReference(i).updateParameters(*this, freq);
            polyMSEGs.getReference(i).process(numSamples);
            state.modMatrix.setPolyValue(*this, state.modSrcPolyMSEG[i], polyMSEGs.getReference(i).getOutput(0), 0);
            state.modMatrix.setPolyValue(*this, state.modSrcPolyMSEG[i], polyMSEGs.getReference(i).getOutput(1), 1);
        }
        else
        {
            state.modMatrix.setPolyValue(*this, state.modSrcPolyMSEG[i], 0, 0);
            state.modMatrix.setPolyValue(*this, state.modSrcPolyMSEG[i], 0, 1);
        }
    }

    for (auto* resonatorBank : resonatorBanks)
    {
        resonatorBank->updateParameters(frequency, numSamples);
    }

    exciterRack.updateParameters();

    bypassResonators = state.bypassResonators;
    gain = getValue(params.masterGain);
}

void ResonatorVoice::renderNextBlock(juce::AudioBuffer<float>& outputBuffer, int startSample, int numSamples)
{
    this->currentBlockStartSample = startSample;
    this->currentBlockNumSamples = numSamples;
    updateParameters(numSamples);

    juce::dsp::AudioBlock<float> exciterBlock = juce::dsp::AudioBlock<float>(exciterBuffer)
        .getSubBlock(startSample, numSamples);
    exciterBlock.clear();

    juce::dsp::AudioBlock<float> outputBlock = juce::dsp::AudioBlock<float>(outputBuffer)
        .getSubBlock(startSample, numSamples);

    //the output of all banks is summed into this block, which is sent to the effect chain
    juce::dsp::AudioBlock<float> tempOutputBlock = juce::dsp::AudioBlock<float>(tempBuffer)
        .getSubBlock(startSample, numSamples);
    tempOutputBlock.clear();

    juce::dsp::AudioBlock<float> soloOutputBlock = juce::dsp::AudioBlock<float>(soloBuffer)
        .getSubBlock(startSample, numSamples);
    soloOutputBlock.clear();

    exciterRack.process(exciterBlock, tempOutputBlock,
                        { state.externalInputFrame, currentSourceStartSample });

    if (!bypassResonators)
    {
        jassert(waveguideScratchArena.isPrepared());
        if (waveguideScratchArena.isPrepared())
        {
            auto previousRoutedBlock = waveguideScratchArena.getBlock(
                WaveguideScratchArena::Region::previousBank, numSamples);
            auto routedOutputBlock = waveguideScratchArena.getBlock(
                WaveguideScratchArena::Region::outputBank, numSamples);
            previousRoutedBlock.clear();

            for (int i = 0; i < resonatorBanks.size(); ++i)
            {
                routedOutputBlock.clear();
                if (!resonatorBanks[i]->isEmptyForRouting())
                {
                    resonatorBanks[i]->processRouted(exciterBlock, previousRoutedBlock,
                                                     routedOutputBlock);
                    dcBlockers[i].process(juce::dsp::ProcessContextReplacing<float>(routedOutputBlock));
                    tempOutputBlock.add(routedOutputBlock);
                }
                std::swap(previousRoutedBlock, routedOutputBlock);
            }
        }
    }

    // Tail retirement is based only on this voice's post-gain signal. Never
    // inspect the shared synth bus: a different voice must not keep this one
    // alive. The quiet window covers the longest unprocessed waveguide period,
    // and is independent of the single global effect chain.
    auto& privateBlock = bypassResonators ? exciterBlock : (state.soloActive ? soloOutputBlock : tempOutputBlock);
    privateBlock.multiplyBy(gain);

    float maxMagnitude = 0.0f;
    bool invalid = false;
    for (size_t channel = 0; channel < privateBlock.getNumChannels(); ++channel)
    {
        const auto* samples = privateBlock.getChannelPointer(channel);
        for (size_t sample = 0; sample < privateBlock.getNumSamples(); ++sample)
        {
            const auto value = samples[sample];
            if (!std::isfinite(value) || std::abs(value) > 100.0f)
            {
                invalid = true;
                break;
            }
            maxMagnitude = juce::jmax(maxMagnitude, std::abs(value));
        }
        if (invalid)
            break;
    }

    if (invalid)
    {
        // Gin owns sanitisation and retirement. Preserve the invalid sample in
        // its private scratch so it can retire this assignment with the
        // correct reason before anything reaches the shared mix.
    }
    else if (noteReleased)
    {
        constexpr float tailThreshold = 0.0000316227766f; // -90 dBFS
        if (maxMagnitude < tailThreshold)
            quietTailSamples += numSamples;
        else
            quietTailSamples = 0;

        constexpr double quietSeconds = 0.5;
        const auto requiredQuietSamples = static_cast<int64_t>(std::ceil(getSampleRate() * quietSeconds));
        if (quietTailSamples >= requiredQuietSamples)
            finishTail();
    }

    outputBlock.add(privateBlock);
    numBlocksSinceNoteOn++;

    finishBlock(numSamples);
}

void ResonatorVoice::renderVoice(gin::VoiceRenderContext& context)
{
    currentSourceStartSample = context.sourceStartSample;
    renderNextBlock(context.buffer, context.startSample, context.numSamples);
}

void ResonatorVoice::voiceRetired(gin::VoiceEndReason)
{
    exciterRack.voiceRetired();
    stopVoice();
    noteReleased = false;
    quietTailSamples = 0;
    numBlocksSinceNoteOn = 0;
}

void ResonatorVoice::notePressureChanged()
{
    auto note = getCurrentlyPlayingNote();
    state.modMatrix.setPolyValue(*this, state.modSrcPressure, note.pressure.asUnsignedFloat(), 0);
    state.modMatrix.setPolyValue(*this, state.modSrcPressure, note.pressure.asUnsignedFloat(), 1);
}

void ResonatorVoice::noteTimbreChanged()
{
    auto note = getCurrentlyPlayingNote();
    state.modMatrix.setPolyValue(*this, state.modSrcTimbre, note.timbre.asUnsignedFloat(), 0);
    state.modMatrix.setPolyValue(*this, state.modSrcTimbre, note.timbre.asUnsignedFloat(), 1);
}

void ResonatorVoice::notePitchbendChanged()
{
    // auto note = getCurrentlyPlayingNote();
    // processor.modMatrix.setPolyValue(*this, processor.modSrcPitchbend, note.pitchbend.asUnsignedFloat());
}

void ResonatorVoice::noteKeyStateChanged()
{
}

float ResonatorVoice::getCurrentNote()
{
    return noteSmoother.getCurrentValue() * 127.0f;
}
