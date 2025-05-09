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
        float lastRandomValue = 0; //the previous "step" in the random walk
        float nextRandomValue = 0; //the next "step" in the random walk
        float currentRandomValue = 0; //the current value at the end of the block of samples, interpolated between last and next
        std::atomic<float> atomicState = 0; //used for atomic reads from the UI thread
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
    void process(int numSamples);
    float getOutput();
    float getOutput(int channel);

    gin::ModVoice* voice = nullptr;
    RandomLFOParams params;
    int numChannels;
    double sampleRate;
    float oneOverSampleRate;
    bool stereo = false;
    RandomState sideStates[2];
    RandomState centerState;
    RandomState& leftState; //convenient aliases
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
