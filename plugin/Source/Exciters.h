#ifndef EXCITERS_H
#define EXCITERS_H

#include <JuceHeader.h>
#include "dsp/MultiFilter.h"

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
    unsigned int noiseSeed = 22222;
};

/**
 * Base class for all Exciters. Each Voice has several Exciters, which are called in sequence to excite the Resonators.
 */

class ResonatorVoice;

class Exciter
{
public:

    explicit Exciter(ResonariumProcessor& proc, gin::ModVoice& voice) : proc(proc), voice(voice) {sampleRate = 44100; maximumBlockSize = 512;}

    virtual ~Exciter()
    = default;

    virtual void nextSample() = 0;

    /**
     * Processes a block of audio samples additively.
     * Exciter audio is added non-destructively to the existing
     * samples in the buffer.
     */
    virtual void process(juce::dsp::AudioBlock<float>& block) = 0;
    virtual void reset() = 0;
    virtual void noteStarted() = 0;
    virtual void noteStopped(bool avoidTailOff) = 0;
    virtual void updateParameters() = 0;

    virtual void prepare(const juce::dsp::ProcessSpec& spec)
    {
        this->sampleRate = spec.sampleRate;
        this->maximumBlockSize = spec.maximumBlockSize;
        jassert(spec.numChannels == 2); //exciters are stereo
    }

    ResonariumProcessor& proc;
    //A pointer to the parent Voice. No need to worry about leaking this, as the voice owns the Exciter.
    gin::ModVoice& voice;
    float sampleRate;
    float maximumBlockSize;
};

/**
 * An exciter that fires off a single impulse at the beginning of the note.
 * Several DSP tools are provided to shape the character of the impulse.
 */
class ImpulseExciter : public Exciter
{
public:
    ImpulseExciter(ResonariumProcessor& proc, gin::ModVoice& voice, ImpulseExciterParams params) : Exciter(proc, voice), params(params), filter(&voice, params.filterParams, false){}

    void prepare(const juce::dsp::ProcessSpec& spec) override;
    void nextSample() override;
    void process(juce::dsp::AudioBlock<float>& block) override;
    void reset() override;
    void noteStarted() override;
    void noteStopped(bool avoidTailOff) override;
    void updateParameters() override;

    ImpulseExciterParams params;
    MultiFilter filter;
    int impulsesRemaining;
    juce::AudioBuffer<float> scratchBuffer;
    juce::dsp::AudioBlock<float> scratchBlock;
    float level;
    int thickness;
    bool polarityFlag = false;
};

/**
 * An exciter that generates noise to excite the resonators.
 * Noise can be shaped by a filter and an ADSR envelope.
 */
class NoiseExciter : public Exciter
{
public:
    NoiseExciter(ResonariumProcessor& proc, gin::ModVoice& voice, NoiseExciterParams params) : Exciter(proc, voice), params(params), filter(&voice, params.filterParams, false){}

    void prepare(const juce::dsp::ProcessSpec& spec) override;
    void nextSample() override;
    void process(juce::dsp::AudioBlock<float>& block) override;
    void reset() override;
    void noteStarted() override;
    void noteStopped(bool avoidTailOff) override;
    void updateParameters() override;

    NoiseExciterParams params;
    NoiseGenerator noise;
    gin::AnalogADSR envelope;
    MultiFilter filter;
    juce::AudioBuffer<float> scratchBuffer;
    juce::dsp::AudioBlock<float> scratchBlock;
};

/**
 * Generates various types of impulse trains.
 * IMPULSE:
 *      Generates a basic impulse train.
 *      MODIFIER changes the thickness of each impulse.
 *      ENTROPY applies randomness to the polarity of each impulse.
 *
 * PULSE:
 *      Generates a shaped pulse.
 *      MODIFIER changes the shape of the pulse.
 *      ENTROPY applies randomness to the pulse
 *
 * NOISE_PULSE:
 *      Generates a noisy burst.
 *      MODIFIER changes the shape of the burst envelope.
 *      ENTROPY applies nonlinear scaling to eahc sample
 */
class ImpulseTrainExciter : public Exciter
{
public:

    enum Mode
    {
        IMPULSE,
        STATIC,
        PULSE,
        NOISE_BURST
    };

    ImpulseTrainExciter(ResonariumProcessor& proc, gin::ModVoice& voice, ImpulseTrainExciterParams params) : Exciter(proc, voice), params(params), filter(&voice, params.filterParams, false){}

    void prepare(const juce::dsp::ProcessSpec& spec) override;
    void nextSample() override;
    void process(juce::dsp::AudioBlock<float>& block) override;
    void reset() override;
    void noteStarted() override;
    void noteStopped(bool avoidTailOff) override;
    void updateParameters() override;

    ImpulseTrainExciterParams params;
    NoiseGenerator noise;
    gin::AnalogADSR envelope;
    MultiFilter filter;
    juce::AudioBuffer<float> scratchBuffer;
    juce::dsp::AudioBlock<float> scratchBlock;
    juce::Random rng;

    Mode mode = IMPULSE;
    float character;
    float entropy;
    int periodInSamples = -1;
    int samplesSinceLastImpulse = -1;
    float staticProbability;

    //for the IMPULSE mode -- how long is the impulse, and how many impulses left to send out?
    int impulseLength = -1;
    int impulsesLeft = -1;
};

/**
 * An exciter that reads audio from a file to excite the resonators.
 * The sample can be shaped by a filter and an ADSR envelope.
 */
// class SampleExciter : public Exciter
// {
// public:
//     void nextSample() override;
//     void process(juce::dsp::AudioBlock<float> block) override;
//     void reset() override;
//     void noteStarted() override;
//     void noteStopped(bool avoidTailOff) override;
//     void updateParameters() override;
// };
//
/**
 * An exciter that funnels real-time audio from the host processor chain to excite the resonators.
 * Notably,
 */
class ExternalInputExciter : public Exciter
{
public:

    ExternalInputExciter(ResonariumProcessor& proc, gin::ModVoice& voice, ExternalInputExciterParams params) : Exciter(proc, voice), params(params), filter(&voice, params.filterParams, false){}

    void prepare(const juce::dsp::ProcessSpec& spec) override;
    void nextSample() override;
    void process(juce::dsp::AudioBlock<float>& block) override;
    void reset() override;
    void noteStarted() override;
    void noteStopped(bool avoidTailOff) override;
    void updateParameters() override;
    void fillExtInputBuffer(juce::AudioBuffer<float> buffer);

    ExternalInputExciterParams params;
    MultiFilter filter;
    juce::AudioBuffer<float> extInBuffer;
    juce::dsp::AudioBlock<float> extInBlock;
};


#endif //EXCITERS_H
