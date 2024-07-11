#include "ResonatorVoice.h"
#include "PluginProcessor.h"

ResonatorVoice::ResonatorVoice(ResonariumProcessor& p, VoiceParams params) : p(p), params(params)
{
    frequency = 440.0f;
    int resonatorBankIndex = 0;

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
        exciters.add(new ImpulseExciter(*this, params.impulseExciterParams[i]));
    }

    for (int i = 0; i < NUM_NOISE_EXCITERS; i++)
    {
        exciters.add(new NoiseExciter(*this, params.noiseExciterParams[i]));
    }

    for (int i = 0; i < NUM_IMPULSE_TRAIN_EXCITERS; i++)
    {
        exciters.add(new ImpulseTrainExciter(*this, params.impulseTrainExciterParams[i]));
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
        l.setSampleRate(spec.sampleRate);
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

    p.modMatrix.setPolyValue(*this, p.modSrcVelocity, note.noteOnVelocity.asUnsignedFloat());
    p.modMatrix.setPolyValue(*this, p.modSrcTimbre, note.initialTimbre.asUnsignedFloat());
    p.modMatrix.setPolyValue(*this, p.modSrcPressure, note.pressure.asUnsignedFloat());

    killIfSilent = false;
    silenceCount = 0;
    numBlocksSinceNoteOn = 0;

    updateParameters(0);
    snapParams();
    updateParameters(0);
    snapParams();

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
}

bool ResonatorVoice::isVoiceActive()
{
    return isActive();
}

void ResonatorVoice::updateParameters(int numSamples)
{
    auto note = getCurrentlyPlayingNote();
    p.modMatrix.setPolyValue(*this, p.modSrcNote, note.initialNote / 127.0f);
    currentMidiNote = noteSmoother.getCurrentValue() * 127.0f;
    currentMidiNote += static_cast<float>(note.totalPitchbendInSemitones);
    frequency = gin::getMidiNoteInHertz(currentMidiNote);

    for (int i = 0; i < NUM_LFOS; i++)
    {
        if (params.lfoParams[i].enabled->isOn())
        {
            gin::LFO::Parameters internalParams;

            float freq = 0;
            if (params.lfoParams[i].sync->getProcValue() > 0.0f)
                freq = 1.0f / gin::NoteDuration::getNoteDurations()[size_t(params.lfoParams[i].beat->getProcValue())].
                    toSeconds(p.getPlayHead());
            else
                freq = p.modMatrix.getValue(params.lfoParams[i].rate);

            internalParams.waveShape = (gin::LFO::WaveShape)int(params.lfoParams[i].wave->getProcValue());
            internalParams.frequency = freq;
            internalParams.phase = p.modMatrix.getValue(params.lfoParams[i].phase);
            internalParams.offset = p.modMatrix.getValue(params.lfoParams[i].offset);
            internalParams.depth = p.modMatrix.getValue(params.lfoParams[i].depth);
            internalParams.delay = 0;
            internalParams.fade = 0;

            polyLFOs[i].setParameters(internalParams);
            polyLFOs[i].process(numSamples);
            p.modMatrix.setPolyValue(*this, p.modSrcPolyLFO[i], polyLFOs[i].getOutput());
        }
        else
        {
            p.modMatrix.setPolyValue(*this, p.modSrcPolyLFO[i], 0);
        }
    }

    for (auto* resonatorBank : resonatorBanks)
    {
        resonatorBank->updateParameters(frequency);
    }

    for (auto* exciter : exciters)
    {
        exciter->updateParameters();
    }

    bypassResonators = p.uiParams.bypassResonators->isOn();
}

void ResonatorVoice::renderNextBlock(juce::AudioBuffer<float>& outputBuffer, int startSample, int numSamples)
{
    //reminder: the output buffer/block is SHARED between voices and may NOT be empty; must ADD only
    updateParameters(numSamples); //important!

    //this block holds the exciter samples. The exciters fill it, then is routed to the resonator banks.
    juce::dsp::AudioBlock<float> exciterBlock = juce::dsp::AudioBlock<float>(exciterBuffer)
        .getSubBlock(startSample, numSamples);
    exciterBlock.clear();

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
    p.modMatrix.setPolyValue(*this, p.modSrcPressure, note.pressure.asUnsignedFloat());
}

void ResonatorVoice::noteTimbreChanged()
{
    auto note = getCurrentlyPlayingNote();
    p.modMatrix.setPolyValue(*this, p.modSrcTimbre, note.timbre.asUnsignedFloat());
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
