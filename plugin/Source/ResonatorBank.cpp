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
            // outSample += r->processSample(input) * r->gain;
            outSample += r->popSample();
            r->pushSample(outSample + input);
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
        // float outSample = 0.0f;
        // float inputSample = input;
        // float totalOutputGain = 0.0f;
        // float totalFeedbackGain = 0.0f;
        //
        // //combine the last outputs of each resonator
        // for(int i = 0; i < NUM_RESONATORS; i++)
        // {
        //     inputSample = inputSample + lastResonatorOutputs[i] * resonators[i]->feedbackGain;
        //     totalFeedbackGain += resonators[i]->feedbackGain;
        // }
        //
        // inputSample = inputSample / totalFeedbackGain;
        //
        // //process each resonator with the combined input sample
        // for(int i = 0; i < NUM_RESONATORS; i++)
        // {
        //     lastResonatorOutputs[i] = resonators[i]->processSample(inputSample);
        //     outSample += lastResonatorOutputs[i] * resonators[i]->gain;
        //     totalOutputGain += resonators[i]->gain;
        // }
        //
        // return outSample / totalOutputGain;

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

    resonators[1]->setHarmonicOffsetInSemitones(7, 0);
    // resonators[1]->gain = 0.9f;
    resonators[2]->setHarmonicOffsetInSemitones(13, 0);
    // resonators[3]->gain = 0.3f;
    // resonators[3]->setHarmonicOffsetInSemitones(-5, 0);
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
