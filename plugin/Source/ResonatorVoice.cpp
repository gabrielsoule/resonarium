#include "ResonatorVoice.h"
#include "PluginProcessor.h"

bool BYPASS_RESONATORS = true; //testing flag to listen to the exciter signal only

ResonatorVoice::ResonatorVoice(ResonariumProcessor& p, VoiceParams params) : processor(p)
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

    processor.modMatrix.setPolyValue(*this, processor.modSrcVelocity, note.noteOnVelocity.asUnsignedFloat());
    processor.modMatrix.setPolyValue(*this, processor.modSrcTimbre, note.initialTimbre.asUnsignedFloat());
    processor.modMatrix.setPolyValue(*this, processor.modSrcPressure, note.pressure.asUnsignedFloat());

    killIfSilent = false;
    silenceCount = 0;
    numBlocksSinceNoteOn = 0;

    updateParameters();
    snapParams();

    for (auto* exciter : exciters)
    {
        exciter->reset();
    }
    for (auto* resonatorBank : resonatorBanks)
    {
        resonatorBank->reset();
    }

    for (auto* exciter : exciters)
    {
        exciter->noteStarted();
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

void ResonatorVoice::updateParameters()
{
    auto note = getCurrentlyPlayingNote();
    processor.modMatrix.setPolyValue(*this, processor.modSrcNote, note.initialNote / 127.0f);
    currentMidiNote = noteSmoother.getCurrentValue() * 127.0f;
    currentMidiNote += static_cast<float>(note.totalPitchbendInSemitones);
    frequency = gin::getMidiNoteInHertz(currentMidiNote);

    for (auto* resonatorBank : resonatorBanks)
    {
        resonatorBank->updateParameters(frequency);
    }

    for (auto* exciter : exciters)
    {
        exciter->updateParameters();
    }
}

void ResonatorVoice::renderNextBlock(juce::AudioBuffer<float>& outputBuffer, int startSample, int numSamples)
{
    //reminder: the output buffer/block is SHARED between voices and may NOT be empty; must ADD only
    updateParameters(); //important!

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

    if (!BYPASS_RESONATORS)
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
        //add the audioblock to the output buffer
        DBG(exciterBuffer.getMagnitude(0, startSample, numSamples));
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
    processor.modMatrix.setPolyValue(*this, processor.modSrcPressure, note.pressure.asUnsignedFloat());
}

void ResonatorVoice::noteTimbreChanged()
{
    auto note = getCurrentlyPlayingNote();
    processor.modMatrix.setPolyValue(*this, processor.modSrcTimbre, note.timbre.asUnsignedFloat());
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
