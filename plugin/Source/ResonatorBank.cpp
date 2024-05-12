//
// Created by Gabriel Soule on 5/8/24.
//

#include "ResonatorBank.h"

ResonatorBank::ResonatorBank()
{
    //add some resonators to the OwnedArray
    for (int i = 0; i < 8; i++)
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
    if (couplingMode == CouplingMode::PARALLEL)
    {
        float sample = 0.0f;
        float totalGain = 0.0f;
        for (auto* r : resonators)
        {
            sample += r->processSample(input);
            totalGain += r->gain;
        }
        sample = sample / totalGain;
        return sample;
    }
    else if (couplingMode == CouplingMode::CASCADE)
    {
        return -1;
    }
    else if (couplingMode == CouplingMode::INTERLINKED)
    {
        return -1;
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

void ResonatorBank::reset()
{
    for (auto* r : resonators)
    {
        r->reset();
    }
}

void ResonatorBank::prepare(const juce::dsp::ProcessSpec& spec)
{
    for (auto* r : resonators)
    {
        r->prepare(spec);
    }

    resonators[1]->setHarmonicOffsetInSemitones(9, 0);
    resonators[2]->setHarmonicOffsetInSemitones(13, 0);
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
