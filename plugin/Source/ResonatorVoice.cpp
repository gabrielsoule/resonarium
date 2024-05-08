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

void ResonatorVoice::prepare(const juce::dsp::ProcessSpec& spec)
{
    DBG("Preparing voice " + juce::String(id));
    MPESynthesiserVoice::setCurrentSampleRate(spec.sampleRate);
    exciterAmpEnv.setSampleRate(spec.sampleRate);
    noteSmoother.setSampleRate(spec.sampleRate);
    resonator.prepare(spec);
    resonator.setMode(Resonator::Mode::Eks);
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

    updateParameters();
    snapParams();

    exciterAmpEnv.reset();
    resonator.reset();
    exciterAmpEnv.noteOn();
    silenceCount = 0;

    // resonator.processSample(0.9f);
}

void ResonatorVoice::noteRetriggered()
{
    DBG("Retriggered note");
}

void ResonatorVoice::noteStopped(bool allowTailOff)
{
    exciterAmpEnv.noteOff();

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
}

bool ResonatorVoice::isVoiceActive()
{
    return isActive();
}

void ResonatorVoice::updateParameters()
{
    auto note = getCurrentlyPlayingNote();
    processor.modMatrix.setPolyValue(*this, processor.modSrcNote, note.initialNote / 127.0f);

    exciterAmpEnv.setAttack(getValue(processor.exciterParams.attack));
    // DBG("Exciter Attack: " + juce::String(getValue(processor.exciterParams.attack)));
    exciterAmpEnv.setDecay(getValue(processor.exciterParams.decay));
    // DBG("Exciter Decay: " + juce::String(getValue(processor.exciterParams.decay)));
    exciterAmpEnv.setSustainLevel(getValue(processor.exciterParams.sustain));
    // DBG("Exciter Sustain: " + juce::String(getValue(processor.exciterParams.sustain)));
    exciterAmpEnv.setRelease(getValue(processor.exciterParams.release));
    // DBG("Exciter Release: " + juce::String(getValue(processor.exciterParams.release)));


    currentMidiNote = noteSmoother.getCurrentValue() * 127.0f;
    currentMidiNote += static_cast<float>(note.totalPitchbendInSemitones);
    frequency = gin::getMidiNoteInHertz(currentMidiNote);
    resonator.setFrequency(frequency);
    //TODO Implement manual tuning

}

void ResonatorVoice::renderNextBlock(juce::AudioBuffer<float>& outputBuffer, int startSample, int numSamples)
{
    updateParameters();
    gin::ScratchBuffer buffer(2, numSamples);

    auto frequency = gin::getMidiNoteInHertz(currentMidiNote);
    // resonator.setFrequency(frequency);
    float maxAmplitude = 0.0f;

    float lastEnvVal;
    for (int i = 0; i < numSamples; i++)
    {
        lastEnvVal = exciterAmpEnv.process();
        const auto exciterSample = noise.nextValue() * lastEnvVal;
        const auto sample = resonator.processSample(exciterSample * 0.5f);
        if (exciterAmpEnv.getState() == gin::ADSR::State::finished)
            maxAmplitude = juce::jmax(maxAmplitude, std::abs(sample));
        outputBuffer.addSample(0, startSample + i, sample);
        outputBuffer.addSample(1, startSample + i, sample);
    }

    // switch (exciterAmpEnv.getState())
    // {
    // case gin::ADSR::State::attack:
    //     DBG("Attack");
    //     break;
    // case gin::ADSR::State::decay:
    //     DBG("Decay");
    //     break;
    // case gin::ADSR::State::sustain:
    //     DBG("Sustain");
    //     break;
    // case gin::ADSR::State::release:
    //     DBG("Release");
    //     break;
    // case gin::ADSR::State::idle:
    //     DBG("Idle");
    //     DBG(lastEnvVal);
    //     break;
    // case gin::ADSR::finished:
    //     DBG("Finished");
    //     DBG(lastEnvVal);
    //     break;
    // }

    // DBG("Max amplitude :: " + juce::String(maxAmplitude));

    if (exciterAmpEnv.getState() == gin::ADSR::State::finished)
    {
        if (maxAmplitude < 0.009f)
        {
            silenceCount += 1;
            if (silenceCount > silenceCountThreshold)
            {
                DBG("Silence detected, stopping note " + juce::String(id));
                stopVoice();
                clearCurrentNote();
            }
        }
        else
        {
            silenceCount = 0;
        }
    }

    finishBlock(numSamples);

    // for (int i = 0; i < numSamples; i++)
    // {
    //     auto sample = noise.nextValue() * exciterAmpEnv.process();
    //     outputBuffer.addSample(0, startSample + i, sample);
    //     outputBuffer.addSample(1, startSample + i, sample);
    // }
    //
    // finishBlock(numSamples);
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
