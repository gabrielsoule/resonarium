//
// Created by Gabriel Soule on 5/8/24.
//

#include "ResonatorBank.h"

ResonatorBank::ResonatorBank()
{
    //add some resonators to the OwnedArray
    for (int i = 0; i < 2; i++)
    {
        resonators.add(new Resonator());
    }

    resonators[1]->setHarmonicOffsetInSemitones(6, 0);
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
}

void ResonatorBank::setFrequency(float frequency)
{
    this->frequency = frequency;
    for (auto* r : resonators)
    {
        r->setFrequency(frequency);
    }
}

void ResonatorBank::setFeedbackMode(CouplingMode newMode)
{
    this->couplingMode = newMode;
}
