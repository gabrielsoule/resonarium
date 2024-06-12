//
// Created by Gabriel Soule on 5/1/24.
//

#ifndef RESONATORVOICE_H
#define RESONATORVOICE_H

#pragma once

#include <JuceHeader.h>
#include "defines.h"
#include "Exciters.h"
#include "ResonatorBank.h"

class NoiseGenerator
{
public:
    void reset()
    {
        noiseSeed = 22222;
    }

    float nextValue()
    {
        noiseSeed = noiseSeed * 196314165 + 907633515;
        int temp = int(noiseSeed >> 7) - 16777216;
        return (float(temp) / 16777216.0f);
    }

private:
    unsigned int noiseSeed;
};

class ResonariumProcessor;


class ResonatorVoice : public gin::SynthesiserVoice, public gin::ModVoice
{
public:
    ResonatorVoice(ResonariumProcessor& p);
    ~ResonatorVoice() override;
    void noteStarted() override;
    void noteStopped(bool allowTailOff) override;
    void noteRetriggered() override;
    float getCurrentNote() override;
    void notePressureChanged() override;
    void noteTimbreChanged() override;
    void notePitchbendChanged() override;
    void noteKeyStateChanged() override;
    // void setCurrentSampleRate(double newRate) override;
    void prepare(const juce::dsp::ProcessSpec& spec);
    void updateParameters();
    void renderNextBlock(juce::AudioBuffer<float>& outputBuffer, int startSample, int numSamples) override;
    bool isVoiceActive() override;

    ResonariumProcessor& processor;
    float frequency;
    gin::ADSR exciterAmpEnv;
    NoiseGenerator noise = NoiseGenerator();
    gin::EasedValueSmoother<float> noteSmoother;
    float currentMidiNote;
    int id;
    juce::OwnedArray<ResonatorBank> resonatorBanks;
    int silenceCount = 0;
    int silenceCountThreshold = 50; //how many quiet samples before we stop the voice?

    ImpulseExciter impulseExciter;
};


#endif //RESONATORVOICE_H
