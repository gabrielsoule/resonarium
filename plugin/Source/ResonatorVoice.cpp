#include "ResonatorVoice.h"
#include "PluginProcessor.h"

ResonatorVoice::ResonatorVoice(ResonariumProcessor& p, VoiceParams params) : proc(p), params(params),
                                                                             effectChain(p, 0, params.effectChainParams)
{
    frequency = 440.0f;
    int resonatorBankIndex = 0;
    this->disableSmoothing = true;

    polyMSEGs.clear();

    juce::dsp::IIR::Coefficients<float>::Ptr dcBlockerCoefficients =
        new juce::dsp::IIR::Coefficients<float>(1, -1, 1, -0.995f);

    for (int i = 0; i < NUM_RESONATOR_BANKS; i++)
    {
        auto* waveguideBank = new WaveguideResonatorBank(*this, params.waveguideResonatorBankParams[i]);
        resonatorBanks.add(waveguideBank);
        resonatorBankIndex++;
        dcBlockers[i].state = dcBlockerCoefficients;
    }

    for (int i = 0; i < NUM_IMPULSE_EXCITERS; i++)
    {
        exciters.add(new ImpulseExciter(proc, *this, params.impulseExciterParams[i]));
    }

    for (int i = 0; i < NUM_NOISE_EXCITERS; i++)
    {
        exciters.add(new NoiseExciter(proc, *this, params.noiseExciterParams[i]));
    }

    for (int i = 0; i < NUM_IMPULSE_TRAIN_EXCITERS; i++)
    {
        exciters.add(new ImpulseTrainExciter(proc, *this, params.impulseTrainExciterParams[i]));
    }

    exciters.add(new SampleExciter(proc, *this, params.sampleExciterParams));
    exciters.add(extInExciter = new ExternalInputExciter(proc, *this, params.externalInputExciterParams));

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
    exciterBuffer = juce::AudioBuffer<float>(spec.numChannels, spec.maximumBlockSize);
    resonatorBankBuffer = juce::AudioBuffer<float>(spec.numChannels, spec.maximumBlockSize);
    tempBuffer = juce::AudioBuffer<float>(spec.numChannels, spec.maximumBlockSize);
    soloBuffer = juce::AudioBuffer<float>(spec.numChannels, spec.maximumBlockSize);
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
    auto note = getCurrentlyPlayingNote();
    DBG(proc.logPrefix + " Starting note on voice " + juce::String(id) + " with MIDI " + juce::String(note.initialNote) + " at Hz " + juce::String(note.getFrequencyInHertz()));
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

    proc.modMatrix.setPolyValue(*this, proc.modSrcVelocity, note.noteOnVelocity.asUnsignedFloat(), 0);
    proc.modMatrix.setPolyValue(*this, proc.modSrcVelocity, note.noteOnVelocity.asUnsignedFloat(), 1);
    proc.modMatrix.setPolyValue(*this, proc.modSrcTimbre, note.initialTimbre.asUnsignedFloat(), 0);
    proc.modMatrix.setPolyValue(*this, proc.modSrcTimbre, note.initialTimbre.asUnsignedFloat(), 1);
    proc.modMatrix.setPolyValue(*this, proc.modSrcPressure, note.pressure.asUnsignedFloat(), 0);
    proc.modMatrix.setPolyValue(*this, proc.modSrcPressure, note.pressure.asUnsignedFloat(), 1);

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
        DBG(proc.logPrefix + " Forcefully stopping note " + juce::String(id));
        clearCurrentNote();
        stopVoice();
    }
    else
    {
        DBG(proc.logPrefix + " Released note " + juce::String(id));
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
    proc.modMatrix.setPolyValue(*this, proc.modSrcNote, note.initialNote / 127.0f);
    currentMidiNote = noteSmoother.getCurrentValue() * 127.0f;
    currentMidiNote += static_cast<float>(note.totalPitchbendInSemitones);
    frequency = gin::getMidiNoteInHertz(currentMidiNote);

    for (int i = 0; i < NUM_LFOS; i++)
    {
        if (params.lfoParams[i].enabled->isOn())
        {
            float freq = 0;
            if (params.lfoParams[i].sync->getProcValue() > 0.0f)
                freq = 1.0f / gin::NoteDuration::getNoteDurations()[size_t(params.lfoParams[i].beat->getProcValue())].
                    toSeconds(proc.getPlayHead());
            else
                freq = getValue(params.lfoParams[i].rate);
            polyLFOs[i].updateParameters(*this, freq);
            polyLFOs[i].process(numSamples);
            //TODO optimize this call to avoid two searches to getreference
            proc.modMatrix.setPolyValue(*this, proc.modSrcPolyLFO[i], polyLFOs[i].getOutput(0), 0);
            proc.modMatrix.setPolyValue(*this, proc.modSrcPolyLFO[i], polyLFOs[i].getOutput(1), 1);
        }
        else
        {
            proc.modMatrix.setPolyValue(*this, proc.modSrcPolyLFO[i], polyLFOs[i].getOutput(0), 0);
            proc.modMatrix.setPolyValue(*this, proc.modSrcPolyLFO[i], polyLFOs[i].getOutput(1), 1);
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
                    toSeconds(proc.getPlayHead());
            else
                rate = getValue(params.randomLfoParams[i].rate);
            polyRandomLFOs[i].updateParameters(rate);
            polyRandomLFOs[i].process(numSamples);
            proc.modMatrix.setPolyValue(*this, proc.modSrcPolyRND[i], polyRandomLFOs[i].getOutput(0), 0);
            proc.modMatrix.setPolyValue(*this, proc.modSrcPolyRND[i], polyRandomLFOs[i].getOutput(1), 1);
        }
    }

    for (int i = 0; i < NUM_ENVELOPES; i++)
    {
        polyEnvelopes[i].updateParameters(*this);
        polyEnvelopes[i].process(numSamples);
        proc.modMatrix.setPolyValue(*this, proc.modSrcPolyENV[i], polyEnvelopes[i].getOutput(), 0);
        proc.modMatrix.setPolyValue(*this, proc.modSrcPolyENV[i], polyEnvelopes[i].getOutput(), 1);
    }

    for (int i = 0; i < NUM_MSEGS; i++)
    {
        if (params.msegParams[i].enabled->isOn())
        {
            float freq = 0;
            if (params.lfoParams[i].sync->getProcValue() > 0.0f)
                freq = 1.0f / gin::NoteDuration::getNoteDurations()[size_t(params.lfoParams[i].beat->getProcValue())].
                    toSeconds(proc.getPlayHead());
            else
                freq = getValue(params.lfoParams[i].rate);
            polyMSEGs.getReference(i).updateParameters(*this, freq);
            polyMSEGs.getReference(i).process(numSamples);
            proc.modMatrix.setPolyValue(*this, proc.modSrcPolyMSEG[i], polyMSEGs.getReference(i).getOutput(0), 0);
            proc.modMatrix.setPolyValue(*this, proc.modSrcPolyMSEG[i], polyMSEGs.getReference(i).getOutput(1), 1);
        }
        else
        {
            proc.modMatrix.setPolyValue(*this, proc.modSrcPolyMSEG[i], 0, 0);
            proc.modMatrix.setPolyValue(*this, proc.modSrcPolyMSEG[i], 0, 1);
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

    effectChain.updateParameters(*this, frequency);

    bypassResonators = proc.uiParams.bypassResonators->isOn();
}

void ResonatorVoice::renderNextBlock(juce::AudioBuffer<float>& outputBuffer, int startSample, int numSamples)
{
    this->startSample = startSample;
    this->numSamples = numSamples;
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

        if (!proc.synth.soloActive)
        {
            effectChain.process(tempOutputBlock);
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
    }

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
    proc.modMatrix.setPolyValue(*this, proc.modSrcPressure, note.pressure.asUnsignedFloat(), 0);
    proc.modMatrix.setPolyValue(*this, proc.modSrcPressure, note.pressure.asUnsignedFloat(), 1);
}

void ResonatorVoice::noteTimbreChanged()
{
    auto note = getCurrentlyPlayingNote();
    proc.modMatrix.setPolyValue(*this, proc.modSrcTimbre, note.timbre.asUnsignedFloat(), 0);
    proc.modMatrix.setPolyValue(*this, proc.modSrcTimbre, note.timbre.asUnsignedFloat(), 1);
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
