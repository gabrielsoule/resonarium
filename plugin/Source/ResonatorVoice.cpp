#include "ResonatorVoice.h"
#include "PluginProcessor.h"

ResonatorVoice::ResonatorVoice(ResonariumProcessor& p, VoiceParams params) : proc(p), params(params)
{
    frequency = 440.0f;
    int resonatorBankIndex = 0;
    this->disableSmoothing = true;

    polyMSEGs.clear();

    //each resonator bank has two indices:
    //its index among all resonator banks,
    //and its index among its particular type of resonator bank
    for (int i = 0; i < NUM_WAVEGUIDE_RESONATOR_BANKS; i++)
    {
        auto* waveguideBank = new WaveguideResonatorBank(*this, params.waveguideResonatorBankParams[i]);
        waveguideBank->resonatorBankIndex = resonatorBankIndex;
        // waveguideResonatorBanks.add(waveguideBank);
        resonatorBanks.add(waveguideBank);
        resonatorBankIndex++;
    }
    for (int i = 0; i < NUM_MODAL_RESONATOR_BANKS; i++)
    {
        auto* modalBank = new ModalResonatorBank(*this, params.modalResonatorBankParams[i]);
        modalBank->resonatorBankIndex = resonatorBankIndex;
        // modalResonatorBanks.add(modalBank);
        resonatorBanks.add(modalBank);
        resonatorBankIndex++;
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
    exciterBuffer.clear();
    resonatorBankBuffer.clear();
    for (auto* exciter : exciters)
    {
        exciter->prepare(spec);
    }

    for (auto* resonatorBank : resonatorBanks)
    {
        resonatorBank->prepare(spec);
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

    juce::dsp::IIR::Coefficients<float>::Ptr dcBlockerCoefficients =
        new juce::dsp::IIR::Coefficients<float>(1, -1, 1, -0.995f);
    dcBlocker.state = dcBlockerCoefficients;
    dcBlocker.prepare(spec);
}

void ResonatorVoice::noteStarted()
{
    DBG("Starting note on voice " + juce::String(id));
    startVoice();
    auto note = getCurrentlyPlayingNote();
    if (glideInfo.fromNote >= 0 && (glideInfo.glissando || glideInfo.portamento))
    {
        DBG("WARNING: Portamento and glissando are not yet implemented.");
        // noteSmoother.setTime (glideInfo.rate);
        // noteSmoother.setValueUnsmoothed (glideInfo.fromNote / 127.0f);
        // noteSmoother.setValue (note.initialNote / 127.0f);
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

    killIfSilent = false;
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

    exciterBuffer.clear();
    resonatorBankBuffer.clear();

    dcBlocker.reset();
}

void ResonatorVoice::noteRetriggered()
{
    DBG("Retriggered note");
}

void ResonatorVoice::noteStopped(bool allowTailOff)
{
    killIfSilent = true;
    if (!allowTailOff)
    {
        DBG("Forcefully stopping note " + juce::String(id));
        clearCurrentNote();
        stopVoice();
    }
    else
    {
        DBG("Released note " + juce::String(id));
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
                rate = 1.0f / gin::NoteDuration::getNoteDurations()[size_t(getValue(params.randomLfoParams[i].beat))].
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

    bypassResonators = proc.uiParams.bypassResonators->isOn();
}

void ResonatorVoice::renderNextBlock(juce::AudioBuffer<float>& outputBuffer, int startSample, int numSamples)
{
    this->startSample = startSample;
    this->numSamples = numSamples;
    //reminder: the output buffer/block is SHARED between voices and may NOT be empty; must ADD only
    updateParameters(numSamples); //important!

    //this block holds the exciter samples. The exciters fill it, then is routed to the resonator banks.
    juce::dsp::AudioBlock<float> exciterBlock = juce::dsp::AudioBlock<float>(exciterBuffer)
        .getSubBlock(startSample, numSamples);
    exciterBlock.clear();

    //extInExciter.fillExtBufferFromProcessor

    //this block holds the output from the resonator banks. It is added to the main output buffer.
    juce::dsp::AudioBlock<float> resonatorBankOutputBlock = juce::dsp::AudioBlock<float>(resonatorBankBuffer)
        .getSubBlock(startSample, numSamples);
    resonatorBankOutputBlock.clear();

    //this block points to the main output buffer supplied by the synthesizer code.
    //important to NOT clear this block, since it contains audio from the other voices
    juce::dsp::AudioBlock<float> outputBlock = juce::dsp::AudioBlock<float>(outputBuffer)
        .getSubBlock(startSample, numSamples);

    for (auto* exciter : exciters)
    {
        exciter->process(exciterBlock);
    }

// #if JUCE_DEBUG
//
//     juce::Range minmax = exciterBlock.findMinAndMax();
//     if(minmax.getEnd() == 0 || minmax.getStart() == 0)
//     {
//         DBG("Exciter block is empty");
//     }
//     else
//     {
//         DBG("Exciter block min: " + juce::String(minmax.getStart()) + " max: " + juce::String(minmax.getEnd()));
//     }
// #endif


    if (!bypassResonators)
    {
        for (auto* resonatorBank : resonatorBanks)
        {
            resonatorBank->process(exciterBlock, resonatorBankOutputBlock);
        }

        //add the resonator banks' output to the main synth output
        dcBlocker.process(juce::dsp::ProcessContextReplacing<float>(resonatorBankOutputBlock));
        outputBlock.add(resonatorBankOutputBlock);
    }
    else
    {
        //add the raw exciter output to the output buffer without the DC blocker
        outputBlock.add(exciterBlock);
    }

    //do silence detection, since the resonators can be unpredictable
    float maxAmplitude = resonatorBankBuffer.getMagnitude(0, startSample, numSamples);
    if (killIfSilent && numBlocksSinceNoteOn > 10)
    {
        if (maxAmplitude < 0.0002f)
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
            DBG("Amplitude overflow detected, stopping note " + juce::String(id));
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
    proc.modMatrix.setPolyValue(*this, proc.modSrcPressure, note.pressure.asUnsignedFloat());
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
