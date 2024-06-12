#ifndef EXCITERS_H
#define EXCITERS_H
#include <JuceHeader.h>

#include "ResonatorBank.h"
#include "dsp/MultiFilter.h"

/**
 * Base class for all Exciters. Each Voice has several Exciters, which are called in sequence to excite the Resonators.
 */

class ResonatorVoice;

class Exciter
{
public:
    virtual ~Exciter()
    = default;

    virtual void nextSample() = 0;

    /**
     * Processes a block of audio samples additively.
     * Exciter audio is added non-destructively to the existing
     * samples in the buffer.
     */
    virtual void process(juce::dsp::AudioBlock<float> block) = 0;
    virtual void reset() = 0;
    virtual void noteStarted() = 0;
    virtual void noteStopped(bool avoidTailOff) = 0;
    virtual void updateParameters() = 0;

    virtual void prepare(const juce::dsp::ProcessSpec& spec, ResonatorVoice* voice)
    {
        this->voice = voice;
        this->sampleRate = spec.sampleRate;
        this->maximumBlockSize = spec.maximumBlockSize;
        jassert(spec.numChannels == 2); //exciters are stereo
    }

    //A pointer to the parent Voice. No need to worry about leaking this, as the voice owns the Exciter.
    ResonatorVoice* voice;
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
    void prepare(const juce::dsp::ProcessSpec& spec, ResonatorVoice* voice) override;
    void nextSample() override;
    void process(juce::dsp::AudioBlock<float> block) override;
    void reset() override;
    void noteStarted() override;
    void noteStopped(bool avoidTailOff) override;
    void updateParameters() override;

    ImpulseExciterParams params;
    MultiFilter filter;
    int impulseLength;
    int impulseCounter;
    bool firstBlock;
    juce::AudioBuffer<float> scratchBuffer;
    juce::dsp::AudioBlock<float> scratchBlock;
};

class ImpulseTrainExciter : public Exciter
{
public:
    void nextSample() override;
    void process(juce::dsp::AudioBlock<float> block) override;
    void reset() override;
    void noteStarted() override;
    void noteStopped(bool avoidTailOff) override;
    void updateParameters() override;
};

/**
 * An exciter that generates noise to excite the resonators.
 * Noise can be shaped by a filter and an ADSR envelope.
 */
class NoiseExciter : public Exciter
{
public:
    void nextSample() override;
    void process(juce::dsp::AudioBlock<float> block) override;
    void reset() override;
    void noteStarted() override;
    void noteStopped(bool avoidTailOff) override;
    void updateParameters() override;


    gin::Filter filter;
    int maxImpulseLength = 500; //stop, he's already dead! (after this many samples)
};

/**
 * An exciter that reads audio from a file to excite the resonators.
 * The sample can be shaped by a filter and an ADSR envelope.
 */
class SampleExciter : public Exciter
{
public:
    void nextSample() override;
    void process(juce::dsp::AudioBlock<float> block) override;
    void reset() override;
    void noteStarted() override;
    void noteStopped(bool avoidTailOff) override;
    void updateParameters() override;
};

/**
 * An exciter that funnels real-time audio from the host processor chain to excite the resonators.
 */
class AudioInputExciter : public Exciter
{
public:
    void nextSample() override;
    void process(juce::dsp::AudioBlock<float> block) override;
    void reset() override;
    void noteStarted() override;
    void noteStopped(bool avoidTailOff) override;
    void updateParameters() override;
};


#endif //EXCITERS_H
