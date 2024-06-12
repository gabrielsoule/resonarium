//
// Created by Gabriel Soule on 5/1/24.
//

#include "ResonatorVoice.h"
#include "PluginProcessor.h"

ResonatorVoice::ResonatorVoice(ResonariumProcessor& p) : processor(p)
{
    noise.reset();
    frequency = 440.0f;
    for(int i = 0; i < NUM_RESONATOR_BANKS; i++)
    {
        resonatorBanks.add(new ResonatorBank(*this));
    }
}

ResonatorVoice::~ResonatorVoice()
{
    resonatorBanks.clear(true);
}

void ResonatorVoice::prepare(const juce::dsp::ProcessSpec& spec)
{
    MPESynthesiserVoice::setCurrentSampleRate(spec.sampleRate);
    exciterAmpEnv.setSampleRate(spec.sampleRate);
    noteSmoother.setSampleRate(spec.sampleRate);
    impulseExciter.prepare(spec, this);
    for(int i = 0; i < NUM_RESONATOR_BANKS; i++)
    {
        jassert(resonatorBanks[i]->params.index == i); //ensure that parameters have been correctly distributed
        resonatorBanks[i]->prepare(spec);
    }
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
    impulseExciter.reset();
    for(int i = 0; i < NUM_RESONATOR_BANKS; i++)
        resonatorBanks[i]->reset();
    exciterAmpEnv.noteOn();
    silenceCount = 0;

    impulseExciter.noteStarted();
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

    impulseExciter.noteStopped(allowTailOff);
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
    exciterAmpEnv.setDecay(getValue(processor.exciterParams.decay));
    exciterAmpEnv.setSustainLevel(getValue(processor.exciterParams.sustain));
    exciterAmpEnv.setRelease(getValue(processor.exciterParams.release));

    currentMidiNote = noteSmoother.getCurrentValue() * 127.0f;
    currentMidiNote += static_cast<float>(note.totalPitchbendInSemitones);
    frequency = gin::getMidiNoteInHertz(currentMidiNote);
    for(int i = 0; i < NUM_RESONATOR_BANKS; i++)
        resonatorBanks[i]->updateParameters(frequency);
    impulseExciter.updateParameters();

}

void ResonatorVoice::renderNextBlock(juce::AudioBuffer<float>& outputBuffer, int startSample, int numSamples)
{
    updateParameters(); //important!
    gin::ScratchBuffer buffer(outputBuffer.getNumChannels(), numSamples);
    juce::dsp::AudioBlock<float> block(buffer);
    impulseExciter.process(block);
    //TODO fill the buffer with exciter samples

    float maxAmplitude = 0.0f;

    for (int i = 0; i < numSamples; i++)
    {
        float lastEnvVal = exciterAmpEnv.process();
        // const float exciterSample = noise.nextValue() * lastEnvVal;
        const float exciterSample = block.getSample(0, i);

        //TODO Properly support resonator bank feedback routing; for now, just add everything together
        float sample = 0.0f;
        for(int j = 0; j < NUM_RESONATOR_BANKS; j++)
            sample += resonatorBanks[j]->processSample(exciterSample);

        if (exciterAmpEnv.getState() == gin::ADSR::State::finished)
            maxAmplitude = juce::jmax(maxAmplitude, std::abs(sample));
        outputBuffer.addSample(0, startSample + i, sample);
        outputBuffer.addSample(1, startSample + i, sample);
    }

    if (exciterAmpEnv.getState() == gin::ADSR::State::finished)
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
        else
        {
            silenceCount = 0;
        }
    }

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
