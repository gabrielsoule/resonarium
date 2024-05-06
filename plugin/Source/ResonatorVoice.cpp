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

}

void ResonatorVoice::noteStopped(bool allowTailOff)
{

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

void ResonatorVoice::renderNextBlock(AudioBuffer<float>& outputBuffer, int startSample, int numSamples)
{
    if(exciterAmpEnv.getState() == gin::AnalogADSR::State::idle)
    {
        stopVoice();
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

    DBG("Note value: " + String(currentMidiNote));
    auto frequency = gin::getMidiNoteInHertz(currentMidiNote);
    DBG("Frequency: " + String(frequency));
    //fill the buffer with noise
    for (int sample = 0; sample < numSamples; sample++)
    {
        outputBuffer.setSample(0, startSample + sample, noise.nextValue());
    }

    //process the buffer with the envelope
    exciterAmpEnv.processMultiplying(outputBuffer, startSample, numSamples);


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




