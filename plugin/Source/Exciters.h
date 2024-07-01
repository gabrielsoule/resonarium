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

    Exciter(ResonatorVoice& voice) : voice(voice) {}

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
    virtual void

    updateParameters() = 0;

    virtual void prepare(const juce::dsp::ProcessSpec& spec)
    {
        this->sampleRate = spec.sampleRate;
        this->maximumBlockSize = spec.maximumBlockSize;
        jassert(spec.numChannels == 2); //exciters are stereo
    }

    //A pointer to the parent Voice. No need to worry about leaking this, as the voice owns the Exciter.
    ResonatorVoice& voice;
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
    ImpulseExciter(ResonatorVoice& voice, ImpulseExciterParams params) : Exciter(voice), params(params), filter(params.filterParams, false){}

    void prepare(const juce::dsp::ProcessSpec& spec) override;
    void nextSample() override;
    void process(juce::dsp::AudioBlock<float>& block) override;
    void reset() override;
    void noteStarted() override;
    void noteStopped(bool avoidTailOff) override;
    void distributeParameters(ImpulseExciterParams params);
    void updateParameters() override;

    ImpulseExciterParams params;
    MultiFilter filter;
    int impulsesRemaining;
    juce::AudioBuffer<float> scratchBuffer;
    juce::dsp::AudioBlock<float> scratchBlock;
};

/**
 * An exciter that generates noise to excite the resonators.
 * Noise can be shaped by a filter and an ADSR envelope.
 */
class NoiseExciter : public Exciter
{
public:
    NoiseExciter(ResonatorVoice& voice, NoiseExciterParams params) : Exciter(voice), params(params), filter(params.filterParams, false){}

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
// /**
//  * An exciter that funnels real-time audio from the host processor chain to excite the resonators.
//  */
// class AudioInputExciter : public Exciter
// {
// public:
//     void nextSample() override;
//     void process(juce::dsp::AudioBlock<float> block) override;
//     void reset() override;
//     void noteStarted() override;
//     void noteStopped(bool avoidTailOff) override;
//     void updateParameters() override;
// };


#endif //EXCITERS_H
