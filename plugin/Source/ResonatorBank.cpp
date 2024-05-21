//
// Created by Gabriel Soule on 5/8/24.
//

#include "ResonatorBank.h"

ResonatorBank::ResonatorBank()
{
    //add some resonators to the OwnedArray
    for (int i = 0; i < NUM_RESONATORS; i++)
    {
        resonators.add(new Resonator());
    }

    couplingMode = PARALLEL;

    // resonators[2]->setHarmonicOffsetInSemitones(13, 0);
    // resonators[2]->gain = 0.4f;
}

ResonatorBank::~ResonatorBank()
{
    resonators.clear(true);
}

float ResonatorBank::processSample(float input)
{
    //Process the resonators in parallel with no intra-resonator feedback
    if (couplingMode == CouplingMode::PARALLEL)
    {
        float outSample = 0.0f;
        float totalGain = 0.0f;
        for (auto* r : resonators)
        {
            const float sample = r->popSample();
            outSample += sample;
            r->pushSample(sample + input);
            totalGain += r->gain;
        }
        outSample = outSample / totalGain;
        return outSample;
    }


    else if (couplingMode == CouplingMode::CASCADE)
    {
        return -1;
    }

    //Feed the combined output of all resonators into the input of each resonator
    else if (couplingMode == CouplingMode::INTERLINKED)
    {
        float outSample = 0.0f;
        float feedbackSample =  0.0f;
        float feedbackGainSum = 0.0f;
        float outputGainSum = 0.0f;

        for(int i = 0; i < NUM_RESONATORS; i++)
        {
            float ithResonatorOutput = resonators[i]->popSample();
            feedbackSample += ithResonatorOutput * resonators[i]->feedbackGain;
            outSample += ithResonatorOutput * resonators[i]->gain;
            feedbackGainSum += resonators[i]->feedbackGain;
            outputGainSum += resonators[i]->gain;
        }

        feedbackSample += input; //add the exciter signal, which has a gain of 1 (i.e. goes from -1 to 1 in amplitude)
        // feedbackGainSum += 1;

        feedbackSample = feedbackGainSum != 0 ? feedbackSample / feedbackGainSum : input;

        for(int i = 0; i < NUM_RESONATORS; i++)
        {
            resonators[i]->pushSample(feedbackSample);
        }

        return outSample / outputGainSum;

    }
    else if (couplingMode == CouplingMode::RANDOM)
    {
        return -1;
    }
    else
    {
        DBG("Invalid feedback mode!");
        return -1;
    }
}

/**
 * Resets the internal state of the resonator bank and its resonators.
 * Use when playing a new note, for example.
 */
void ResonatorBank::reset()
{
    for (int i = 0; i < NUM_RESONATORS; i++)
    {
        lastResonatorOutputs[i] = 0.0f;
        resonators[i]->reset();
    }

    jassert(NUM_RESONATORS == resonators.size());

}

void ResonatorBank::prepare(const juce::dsp::ProcessSpec& spec)
{
    for (auto* r : resonators)
    {
        r->prepare(spec);
    }

    if(resonators.size() > 1) resonators[1]->setHarmonicOffsetInSemitones(12, 0);
    if(resonators.size() > 2) resonators[2]->setHarmonicOffsetInSemitones(24, 0);
    if(resonators.size() > 3) resonators[3]->setHarmonicOffsetInSemitones(-12, 0);
    sampleRate = spec.sampleRate;
    reset();

}

void ResonatorBank::setFrequency(float newFrequency)
{
    this->frequency = newFrequency;
    for (auto* r : resonators)
    {
        r->setFrequency(newFrequency);
    }
}

void ResonatorBank::setFeedbackMode(CouplingMode newMode)
{
    this->couplingMode = newMode;
}
