//
// Created by Gabriel Soule on 5/8/24.
//

#include "ResonatorBank.h"
#include "ResonatorVoice.h"

ResonatorBank::ResonatorBank(ResonatorVoice& parentVoice) : voice(parentVoice)
{
    //add some resonators to the OwnedArray
    for (int i = 0; i < NUM_RESONATORS; i++)
    {
        resonators.add(new Resonator(parentVoice));
    }

    couplingMode = PARALLEL;
}

ResonatorBank::~ResonatorBank()
{
    resonators.clear(true);
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
    couplingFilter.reset();
    lastOutput = 0.0f;
    // if(couplingMode == PARALLEL) couplingMode = CouplingMode::INTERLINKED2;
    // else if(couplingMode == INTERLINKED2) couplingMode = PARALLEL;

}

void ResonatorBank::prepare(const juce::dsp::ProcessSpec& spec)
{
    for (int i = 0; i < NUM_RESONATORS; i++)
    {
        jassert(resonators[i]->params.resonatorIndex == i); //ensure that parameters have been correctly distributed
        resonators[i]->prepare(spec);
    }

    couplingFilter.coefficients = juce::dsp::IIR::Coefficients<float>::makeFirstOrderLowPass(spec.sampleRate, 10000);

    if(resonators.size() > 1) resonators[1]->setHarmonicOffsetInSemitones(11, 0);
    if(resonators.size() > 2) resonators[2]->setHarmonicOffsetInSemitones(24, 0);
    if(resonators.size() > 3) resonators[3]->setHarmonicOffsetInSemitones(-12, 0);
    sampleRate = spec.sampleRate;
    reset();

}

/**
 * Updates all internal parameters based on the state of the VST parameters corresponding to this ResonatorBank.
 * Child Resonators are also updated.
 * Frequency is passed as an argument since frequency comes from the MIDI note, not the VST parameters.
 */
void ResonatorBank::updateParameters(float newFrequency)
{
    this->frequency = newFrequency;
    for (auto* r: resonators) r->updateParameters(newFrequency);
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
            outSample += r->processSample(input);

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

    else if (couplingMode == CouplingMode::INTERLINKED2)
    {
        float outSample = 0.0f;
        lastOutput = 0.3f * (couplingFilter.processSample(lastOutput)) / static_cast<float>(NUM_RESONATORS);
        DBG(lastOutput);

        for(auto* r : resonators)
        {
            r->decayCoefficient = 0.8f;
            auto temp = (input + lastOutput);
            outSample += r->processSample(temp);
        }

        lastOutput = outSample;

        return outSample / NUM_RESONATORS;
    }

    else if (couplingMode == CouplingMode::RANDOM)
    {
        auto state0 = resonators[0]->popSample();
        auto state1 = resonators[1]->popSample();

        resonators[0]->pushSample(state1 + input);
        resonators[1]->pushSample(state0 + input);

        return state1;
    }
    else
    {
        DBG("Invalid feedback mode!");
        return -1;
    }
}

void ResonatorBank::setFeedbackMode(CouplingMode newMode)
{
    this->couplingMode = newMode;
}
