//
// Created by Gabriel Soule on 5/1/24.
//

#ifndef RESONATORVOICE_H
#define RESONATORVOICE_H

#pragma once

#include <JuceHeader.h>
#include "Exciters.h"
#include "ResonatorBank.h"

class ResonariumProcessor;


class ResonatorVoice : public gin::SynthesiserVoice, public gin::ModVoice
{
public:
    ResonatorVoice(ResonariumProcessor& p, VoiceParams params);
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
    gin::EasedValueSmoother<float> noteSmoother;
    float currentMidiNote;
    int id = 0;
    juce::OwnedArray<ResonatorBank> resonatorBanks;
    int silenceCount = 0;
    int silenceCountThreshold = 50; //how many quiet samples before we stop the voice?
    int numBlocksSinceNoteOn; // what it says on the tin.
    juce::AudioBuffer<float> exciterBuffer; // buffer for exciters to write to, is routed to resonator banks
    juce::AudioBuffer<float> resonatorBankBuffer; // buffer for resonator banks to write to, is routed to output
    juce::dsp::IIR::Filter<float> dcBlocker;
    juce::OwnedArray<Exciter> exciters;
};


#endif //RESONATORVOICE_H
