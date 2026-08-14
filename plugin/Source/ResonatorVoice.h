#ifndef RESONATORVOICE_H
#define RESONATORVOICE_H

#pragma once

#include "Exciters.h"
#include "WaveguideResonatorBank.h"
#include "util/RandomLFO.h"
#include "util/StereoLFOWrapper.h"
#include "util/StereoMSEGWrapper.h"
#include "util/WrappedEnvelope.h"

class ResonariumProcessor;


class ResonatorVoice : public gin::SynthesiserVoice, public gin::ModVoice
{
public:
    ResonatorVoice(GlobalState& state, VoiceParams params,
                   WaveguideScratchArena& scratchArena);
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
    void renderVoice(gin::VoiceRenderContext& context) override;
    void voiceRetired(gin::VoiceEndReason reason) override;
    bool isVoiceActive() override;

    [[nodiscard]] VoiceExciterRack& getExciterRackForTesting() noexcept { return exciterRack; }
    [[nodiscard]] const VoiceExciterRack& getExciterRackForTesting() const noexcept { return exciterRack; }

    GlobalState& state;
    VoiceParams params;
    WaveguideScratchArena& waveguideScratchArena;
    float frequency = 440.0f;
    gin::EasedValueSmoother<float> noteSmoother;
    float currentMidiNote = 64;
    int id = 0;
    juce::OwnedArray<WaveguideResonatorBank> resonatorBanks;
    float gain = 1.0f;

    int currentBlockStartSample = 0; //start sample of the current block
    int currentSourceStartSample = 0; //matching offset in the host/input block
    int currentBlockNumSamples = 64; //num samples in the current block

    bool noteReleased = false;
    int64_t quietTailSamples = 0;
    int numBlocksSinceNoteOn = 0; // what it says on the tin.

    juce::AudioBuffer<float> exciterBuffer; // buffer for exciters to write to, is routed to resonator banks
    juce::AudioBuffer<float> tempBuffer; // temporary buffer for processing
    juce::AudioBuffer<float> soloBuffer; // if a resonator's solo button is toggled, that resonator's output is routed here

    juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>, juce::dsp::IIR::Coefficients<float>> dcBlockers[NUM_RESONATORS];

    VoiceExciterRack exciterRack;

    std::array<StereoLFOWrapper, NUM_LFOS> polyLFOs;
    std::array<RandomLFO, NUM_RANDOMS> polyRandomLFOs;
    std::array<WrappedEnvelope, NUM_ENVELOPES> polyEnvelopes;
    juce::Array<StereoMSEGWrapper> polyMSEGs; //have to use heap array since mseg has no default constructor,
    juce::NormalisableRange<float> skewedFrequencyRange; //required to appropriately skew the frequency modulation src

    // Constructed/seeding happens off the audio thread. Note starts only
    // advance this local generator; they never access JUCE's system RNG.
    juce::Random noteRandom;

    bool bypassResonators = false;
};


#endif //RESONATORVOICE_H
