//
// Created by Gabriel Soule on 7/10/24.
//

#ifndef RANDOMLFO_H
#define RANDOMLFO_H
#include "../Parameters.h"

/**
* A versatile source of randomness that can be used as a modulator signal.
* Produces either a monophonic or a polyphonic output.
* Can be supplied with an optional RandomLFOParameters struct to control its behavior.
*/
class RandomLFO {
    //Based off of the excellent Perlin random generator in Matt Tytel's Vital synth
    static float perlinInterpolate(float from, float to, float t);
    struct RandomState
    {
        juce::Random rng;
        float lastRandomValue; //the previous "step" in the random walk
        float nextRandomValue; //the next "step" in the random walk
        float currentRandomValue; //the current value at the end of the block of samples, interpolated between last and next
    };

    void processInternal(int numSamples, RandomState& state);

public:

    enum Mode
    {
        SMOOTH,
        STEP,
        SAMPLE_AND_HOLD,
    };
    RandomLFO() : leftState(sideStates[0]), rightState(sideStates[1]){}
    RandomLFO(gin::ModVoice* voice, bool stereo);
    RandomLFO(RandomLFOParams params, bool stereo);
    RandomLFO(gin::ModVoice* voice, RandomLFOParams params, bool stereo);
    void prepare(const juce::dsp::ProcessSpec& spec);
    void updateParameters(float frequency);
    void updateParametersMono(gin::ModMatrix& matrix, float frequency);
    /**
     * Returns the value of the random LFO after numSamples have been processed,
     * and updates its internal state accordingly.
     */
    void reset();
    void noteOn(float phase);
    float process(int numSamples);
    float getOutput();
    float getOutputUnclamped();
    float getOutput(int channel);
    float getOutputUnclamped(int channel);

    gin::ModVoice* voice = nullptr;
    RandomLFOParams params;
    int numChannels;
    double sampleRate;
    float oneOverSampleRate;
    bool stereo = false;
    RandomState sideStates[2];
    RandomState centerState;
    RandomState& leftState; //convienient aliases
    RandomState& rightState;

    float currentPhase;
    float phaseDelta;

    Mode mode = SMOOTH;
    bool sync = false;
    float rate = 1.0f;
    float depth = 1.0f;
    float offset = 0.0f;
    float smooth = 0.0f;
    float chaos = 1.0f;
    float jitter = 0.0f;
    float stereoAmount = 0.0f;
};



#endif //RANDOMLFO_H
