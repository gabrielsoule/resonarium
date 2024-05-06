//
// Created by Gabriel Soule on 5/1/24.
//

#include "ResonatorVoice.h"
#include "PluginProcessor.h"

ResonatorVoice::ResonatorVoice(ResonariumProcessor& p) : processor(p)
{
    noise.reset();
    frequency = 440.0f;
}
ResonatorVoice::~ResonatorVoice()
= default;


void ResonatorVoice::noteStarted()
{
    DBG("Starting new note");
    startVoice();
    auto note = getCurrentlyPlayingNote();
    currentMidiNote = note.initialNote;
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
        noteSmoother.setValueUnsmoothed (note.initialNote / 127.0f);
    }

    processor.modMatrix.setPolyValue (*this, processor.modSrcVelocity, note.noteOnVelocity.asUnsignedFloat());
    processor.modMatrix.setPolyValue (*this, processor.modSrcTimbre, note.initialTimbre.asUnsignedFloat());
    processor.modMatrix.setPolyValue (*this, processor.modSrcPressure, note.pressure.asUnsignedFloat());

    exciterAmpEnv.reset();
    exciterAmpEnv.noteOn();
}

void ResonatorVoice::noteRetriggered()
{
    DBG("Retriggered note");
}

void ResonatorVoice::noteStopped(bool allowTailOff)
{
    DBG("Stopping note");
    exciterAmpEnv.noteOff();

    if (!allowTailOff)
    {
        clearCurrentNote();
        stopVoice();
    }
}

bool ResonatorVoice::isVoiceActive()
{
    return isActive();
}

void ResonatorVoice::setCurrentSampleRate(double newRate)
{
    MPESynthesiserVoice::setCurrentSampleRate(newRate);
    exciterAmpEnv.setSampleRate(newRate);
    noteSmoother.setSampleRate(newRate);
}

void ResonatorVoice::renderNextBlock(juce::AudioBuffer<float>& outputBuffer, int startSample, int numSamples)
{
    if(exciterAmpEnv.getState() == gin::AnalogADSR::State::idle)
    {
        DBG("Envelope idle, terminating note");
        stopVoice();
        clearCurrentNote();
    }

    auto note = getCurrentlyPlayingNote();
    processor.modMatrix.setPolyValue (*this, processor.modSrcNote, note.initialNote / 127.0f);

    exciterAmpEnv.setAttack(getValue(processor.exciterParams.attack));
    exciterAmpEnv.setDecay(getValue(processor.exciterParams.decay));
    exciterAmpEnv.setSustainLevel(getValue(processor.exciterParams.sustain));
    exciterAmpEnv.setRelease(getValue(processor.exciterParams.release));

    currentMidiNote = noteSmoother.getCurrentValue() * 127.0f;
    currentMidiNote += static_cast<float>(note.totalPitchbendInSemitones);
    //TODO Implement manual tuning

    // DBG("Note value: " + juce::String(currentMidiNote));
    auto frequency = gin::getMidiNoteInHertz(currentMidiNote);
    // DBG("Frequency: " + juce::String(frequency));
    //fill the buffer with noise
    for (int i = 0; i < numSamples; i++)
    {
        auto sample = noise.nextValue();
        outputBuffer.addSample(0, startSample + i, sample);
        outputBuffer.addSample(1, startSample + i, sample);
    }

    //process the buffer with the envelope
    exciterAmpEnv.processMultiplying(outputBuffer, startSample, numSamples);
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
    auto note = getCurrentlyPlayingNote();
    processor.modMatrix.setPolyValue(*this, processor.modSrcPitchbend, note.pitchbend.asUnsignedFloat());
}

void ResonatorVoice::noteKeyStateChanged()
{
    DBG("Key state changed; NYI");
}

float ResonatorVoice::getCurrentNote()
{
    return noteSmoother.getCurrentValue() * 127.0f;
}




