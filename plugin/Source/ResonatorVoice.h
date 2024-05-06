//
// Created by Gabriel Soule on 5/1/24.
//

#ifndef RESONATORVOICE_H
#define RESONATORVOICE_H

#pragma once

#include <JuceHeader.h>
class NoiseGenerator
{
public:
    void reset()
    {
        noiseSeed = 22222;
    }

    float nextValue()
    {
        // Generate the next integer pseudorandom number.
        noiseSeed = noiseSeed * 196314165 + 907633515;

        // Convert to a signed value.
        int temp = int(noiseSeed >> 7) - 16777216;

        // Convert to a floating-point number between -1.0 and 1.0.
        return float(temp) / 16777216.0f;
    }

private:
    unsigned int noiseSeed;
};

class ResonariumProcessor;


class ResonatorVoice : public gin::SynthesiserVoice, public gin::ModVoice{
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
    void setCurrentSampleRate(double newRate) override;
    void renderNextBlock(juce::AudioBuffer<float>& outputBuffer, int startSample, int numSamples) override;
    bool isVoiceActive() override;

    ResonariumProcessor& processor;
    float frequency;
    gin::AnalogADSR exciterAmpEnv;
    NoiseGenerator noise = NoiseGenerator();
    gin::EasedValueSmoother<float> noteSmoother;
    float currentMidiNote;
    int id;

};



#endif //RESONATORVOICE_H
