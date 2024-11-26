#ifndef RESONATORVOICE_H
#define RESONATORVOICE_H

#pragma once

#include <JuceHeader.h>
#include "Exciters.h"
#include "WaveguideResonatorBank.h"
#include "dsp/ResonariumEffectChain.h"
#include "util/RandomLFO.h"
#include "util/StereoLFOWrapper.h"
#include "util/StereoMSEGWrapper.h"
#include "util/WrappedEnvelope.h"

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
    void prepare(const juce::dsp::ProcessSpec& spec);
    void updateParameters(int numSamples);
    void renderNextBlock(juce::AudioBuffer<float>& outputBuffer, int startSample, int numSamples) override;
    bool isVoiceActive() override;

    ResonariumProcessor& proc;
    VoiceParams params;
    float frequency = 440.0f;
    gin::EasedValueSmoother<float> noteSmoother;
    float currentMidiNote = 64;
    int id = 0;
    juce::OwnedArray<WaveguideResonatorBank> resonatorBanks;

    int startSample = 0; //start sample of the current block
    int numSamples = 64; //num samples in the current block

    bool noteReleased = false;
    int silenceCount = 0;
    int silenceCountThreshold = 2; //how many quiet samples before we stop the voice?
    int numBlocksSinceNoteOn = 0; // what it says on the tin.

    int testForSilenceBlockPeriod = 15;
    int testForSilenceBlockCount = 0;

    juce::AudioBuffer<float> exciterBuffer; // buffer for exciters to write to, is routed to resonator banks
    juce::AudioBuffer<float> resonatorBankBuffer; // buffer for resonator banks to write to, is routed to output
    juce::AudioBuffer<float> tempBuffer; // temporary buffer for processing
    juce::AudioBuffer<float> soloBuffer; // if a resonator's solo button is toggled, that resonator's output is routed here

    juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>, juce::dsp::IIR::Coefficients<float>> dcBlockers[NUM_RESONATORS];

    juce::OwnedArray<Exciter> exciters;
    ExternalInputExciter* extInExciter; //alias for the external input exciter which requires some special treatment

    std::array<StereoLFOWrapper, NUM_LFOS> polyLFOs;
    std::array<RandomLFO, NUM_RANDOMS> polyRandomLFOs;
    std::array<WrappedEnvelope, NUM_ENVELOPES> polyEnvelopes;
    juce::Array<StereoMSEGWrapper> polyMSEGs; //have to use heap array since mseg has no default constructor,

    ResonariumEffectChain effectChain;

    bool bypassResonators = false;
};


#endif //RESONATORVOICE_H
