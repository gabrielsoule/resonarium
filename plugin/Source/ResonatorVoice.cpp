#include "ResonatorVoice.h"

ResonatorVoice::ResonatorVoice(GlobalState& state, VoiceParams params) : state(state), params(params),
                                                                         effectChain(params.effectChainParams)
{
    frequency = 440.0f;
    this->disableSmoothing = true;

    polyMSEGs.clear();

    juce::dsp::IIR::Coefficients<float>::Ptr dcBlockerCoefficients =
        new juce::dsp::IIR::Coefficients<float>(1, -1, 1, -0.995f);

    for (int i = 0; i < NUM_RESONATOR_BANKS; i++)
    {
        auto* waveguideBank = new WaveguideResonatorBank(state, *this, params.waveguideResonatorBankParams[i]);
        resonatorBanks.add(waveguideBank);
        dcBlockers[i].state = dcBlockerCoefficients;
    }

    for (int i = 0; i < NUM_IMPULSE_EXCITERS; i++)
    {
        exciters.add(new ImpulseExciter(state, *this, params.impulseExciterParams[i]));
    }

    for (int i = 0; i < NUM_NOISE_EXCITERS; i++)
    {
        exciters.add(new NoiseExciter(state, *this, params.noiseExciterParams[i]));
    }

    for (int i = 0; i < NUM_IMPULSE_TRAIN_EXCITERS; i++)
    {
        exciters.add(new SequenceExciter(state, *this, params.impulseTrainExciterParams[i]));
    }

    exciters.add(new SampleExciter(state, *this, params.sampleExciterParams));
    exciters.add(extInExciter = new ExternalInputExciter(state, *this, params.externalInputExciterParams));

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
    exciters.clear(true);
    resonatorBanks.clear(true);
}

void ResonatorVoice::prepare(const juce::dsp::ProcessSpec& spec)
{
    MPESynthesiserVoice::setCurrentSampleRate(spec.sampleRate);
    noteSmoother.setSampleRate(spec.sampleRate);
    
    // Use setSize instead of creating new buffers - more efficient when 
    // prepare() is called multiple times with the same specs
    exciterBuffer.setSize(spec.numChannels, spec.maximumBlockSize, false, true, true);
    resonatorBankBuffer.setSize(spec.numChannels, spec.maximumBlockSize, false, true, true);
    tempBuffer.setSize(spec.numChannels, spec.maximumBlockSize, false, true, true);
    soloBuffer.setSize(spec.numChannels, spec.maximumBlockSize, false, true, true);
    
    // Clear all buffers
    exciterBuffer.clear();
    resonatorBankBuffer.clear();
    tempBuffer.clear();
    soloBuffer.clear();

    for (auto* exciter : exciters)
    {
        exciter->prepare(spec);
    }

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

    effectChain.prepare(spec);
}

void ResonatorVoice::noteStarted()
{
    startVoice();
    // Reset the fastKill flag when starting a new note to fix voice counter bug after panic
    fastKill = false;
    auto note = getCurrentlyPlayingNote();
    DBG(state.logPrefix + " Starting note on voice " + juce::String(id) + " with MIDI " + juce::String(note.initialNote)
        + " at Hz " + juce::String(note.getFrequencyInHertz()));
    if (glideInfo.fromNote >= 0 && (glideInfo.glissando || glideInfo.portamento))
    {
        DBG("WARNING: Portamento and glissando are not yet implemented.");
        stopVoice();
        clearCurrentNote();
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
    silenceCount = 0;
    numBlocksSinceNoteOn = 0;

    snapParams();
    updateParameters(0);

    for (auto* exciter : exciters)
    {
        exciter->reset();
    }
    for (auto* resonatorBank : resonatorBanks)
    {
        resonatorBank->reset();
    }

    for (auto& lfo : polyLFOs)
    {
        lfo.reset();
    }

    for (auto* exciter : exciters)
    {
        exciter->noteStarted();
    }

    for (int i = 0; i < NUM_LFOS; ++i)
    {
        polyLFOs[i].noteOn(
            params.lfoParams[i].retrig->getBoolValue() ? -1 : juce::Random::getSystemRandom().nextFloat());
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

    effectChain.reset();
    testForSilenceBlockCount = 0;
}

void ResonatorVoice::noteRetriggered()
{
    DBG("Retriggered note");
}

void ResonatorVoice::noteStopped(bool allowTailOff)
{
    noteReleased = true;
    if (!allowTailOff)
    {
        DBG(state.logPrefix + " Forcefully stopping note " + juce::String(id));
        clearCurrentNote();
        stopVoice();
    }
    else
    {
        DBG(state.logPrefix + " Released note " + juce::String(id));
    }

    for (auto* exciter : exciters)
    {
        exciter->noteStopped(allowTailOff);
    }

    for (int i = 0; i < NUM_ENVELOPES; i++)
    {
        polyEnvelopes[i].noteOff();
    }
}

bool ResonatorVoice::isVoiceActive()
{
    return isActive();
}

void ResonatorVoice::updateParameters(int numSamples)
{
    auto note = getCurrentlyPlayingNote();
    state.modMatrix.setPolyValue(*this, state.modSrcNote, note.initialNote / 127.0f);
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
            //TODO optimize this call to avoid two searches to getreference
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
            if (params.lfoParams[i].sync->getProcValue() > 0.0f)
                freq = 1.0f / gin::NoteDuration::getNoteDurations()[size_t(params.lfoParams[i].beat->getProcValue())].
                    toSeconds(state.playHead);
            else
                freq = getValue(params.lfoParams[i].rate);
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

    for (auto* exciter : exciters)
    {
        exciter->updateParameters();
    }

    effectChain.updateParameters(*this, state.playHead);
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

    juce::dsp::AudioBlock<float> previousBankBlock = juce::dsp::AudioBlock<float>(resonatorBankBuffer)
        .getSubBlock(startSample, numSamples);
    previousBankBlock.clear();

    //the output of all banks is summed into this block, which is sent to the effect chain
    juce::dsp::AudioBlock<float> tempOutputBlock = juce::dsp::AudioBlock<float>(tempBuffer)
        .getSubBlock(startSample, numSamples);
    tempOutputBlock.clear();

    for (auto* exciter : exciters)
    {
        exciter->process(exciterBlock, tempOutputBlock);
    }

    if (!bypassResonators)
    {
        for (int i = 0; i < resonatorBanks.size(); i++)
        {
            auto* resonatorBank = resonatorBanks[i];

            resonatorBank->process(exciterBlock, previousBankBlock);
            dcBlockers[i].process(juce::dsp::ProcessContextReplacing<float>(previousBankBlock));
            tempOutputBlock.add(previousBankBlock);
        }

        if (!state.soloActive)
        {
            if (state.polyFX)
            {
                effectChain.process(tempOutputBlock);
            }
            outputBlock.add(tempOutputBlock);
        }
        else //if a solo is active, discard the full output and just send out the solo block
        {
            juce::dsp::AudioBlock<float> soloOutputBlock = juce::dsp::AudioBlock<float>(soloBuffer)
                .getSubBlock(startSample, numSamples);
            outputBlock.add(soloOutputBlock);
        }
    }
    else
    {
        outputBlock.add(exciterBlock);
        DBG("Bypasing");
        DBG(outputBlock.findMinAndMax().getEnd());
    }

    outputBlock.multiplyBy(gain);

    //Silence detection code
    testForSilenceBlockCount++;
    if (testForSilenceBlockCount > testForSilenceBlockPeriod && noteReleased && numBlocksSinceNoteOn > 10)
    {
        testForSilenceBlockCount = 0;
        float maxAmplitude = outputBuffer.getMagnitude(startSample, numSamples);
        if (maxAmplitude < 0.001f)
        {
            silenceCount += 1;
            if (silenceCount > silenceCountThreshold)
            {
                DBG("Silence detected, stopping note " + juce::String(id));
                stopVoice();
                clearCurrentNote();
            }
        }
        else if (maxAmplitude > 100.0f)
        {
            DBG("Amplitude overflow detected in silence detection code, stopping note " + juce::String(id));
            stopVoice();
            clearCurrentNote();
        }
        else
        {
            silenceCount = 0;
        }
    }
    numBlocksSinceNoteOn++;

    finishBlock(numSamples);
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
    DBG("Key state changed; NYI");
}

float ResonatorVoice::getCurrentNote()
{
    return noteSmoother.getCurrentValue() * 127.0f;
}
