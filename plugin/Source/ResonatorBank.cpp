//
// Created by Gabriel Soule on 5/8/24.
//

#include "ResonatorBank.h"
#include "ResonatorVoice.h"

ResonatorBank::ResonatorBank(ResonatorVoice& parentVoice, ResonatorBankParams params) : voice(parentVoice), params(params)
{
    //add some resonators to the OwnedArray
    for (int i = 0; i < NUM_RESONATORS; i++)
    {
        DBG(parentVoice.id);
        resonators.add(new Resonator(parentVoice, params.resonatorParams[i]));
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
    this->couplingMode = static_cast<CouplingMode>(voice.getValue(params.couplingMode));
    for (auto* r: resonators) r->updateParameters(newFrequency);

}

[[deprecated("Use process(...) instead for amortized efficiency")]]
float ResonatorBank::processSample(float input)
{
    //Process the resonators in parallel with no intra-resonator feedback
    if (couplingMode == CouplingMode::PARALLEL)
    {
        float outSample = 0.0f;
        float totalGain = 0.0f;
        for (auto* r : resonators)
        {
            outSample += r->processSample(input) * r->gain;
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


        float resonatorOutSamples[NUM_RESONATORS];

        float outSample = 0.0f;
        float feedbackSample =  0.0f;

        for(int i = 0; i < NUM_RESONATORS; i++)
        {
            resonatorOutSamples[i] = resonators[i]->popSample();
            auto bridgeGain = 1.0f;
            //main invariant: the feedbackSample maximum amplitude must be -2.0f.
            //Hence, the gain of each resonator is weighted by the number of resonators and their respective gains.

            feedbackSample += resonatorOutSamples[i];
            outSample += resonatorOutSamples[i];
        }

        feedbackSample = -1.0f * feedbackSample; //apply the bridge filter, which for now is H(z)=-1 :-)
        // feedbackSample += input;
        // feedbackSample = 0.0f;

        for(int i = 0; i < NUM_RESONATORS; i++)
        {
            resonators[i]->pushSample(feedbackSample + resonatorOutSamples[i] + input);
        }
        //
        return outSample;

    }

    // else if (couplingMode == CouplingMode::INTERLINKED2)
    // {
    //     float outSample = 0.0f;
    //     lastOutput = 0.3f * (couplingFilter.processSample(lastOutput)) / static_cast<float>(NUM_RESONATORS);
    //     DBG(lastOutput);
    //
    //     for(auto* r : resonators)
    //     {
    //         r->decayCoefficient = 0.8f;
    //         auto temp = (input + lastOutput);
    //         outSample += r->processSample(temp);
    //     }
    //
    //     lastOutput = outSample;
    //
    //     return outSample / NUM_RESONATORS;
    // }
    //
    // else if (couplingMode == CouplingMode::RANDOM)
    // {
    //     auto state0 = resonators[0]->popSample();
    //     auto state1 = resonators[1]->popSample();
    //
    //     resonators[0]->pushSample(state1 + input);
    //     resonators[1]->pushSample(state0 + input);
    //
    //     return state1;
    // }
    else
    {
        DBG("Invalid feedback mode!");
        return -1;
    }
}

void ResonatorBank::process(juce::dsp::AudioBlock<float>& exciterBlock, juce::dsp::AudioBlock<float>& outputBlock)
{
    //reminder: the outputBlock may already have samples inside...
    jassert(exciterBlock.getNumSamples() == outputBlock.getNumSamples());
    jassert(exciterBlock.getNumChannels() == outputBlock.getNumChannels());

    //compute the total gain so that the output can be normalized,
    //used by all resonator bank processing modes
    float totalGain = 0.0f;
    for (auto* r : resonators)
    {
        totalGain += r->gain;
    }

    if(totalGain == 0.0f) return;

    if(couplingMode == PARALLEL)
    {
        for(int i = 0; i < exciterBlock.getNumSamples(); i++)
        {
            float outSample = 0.0f;
            for (auto* r : resonators)
            {
                outSample += r->processSample(exciterBlock.getSample(0, i)) * r->gain;
            }
            outSample = outSample / totalGain;
            outputBlock.addSample(0, i, outSample);
            outputBlock.addSample(1, i, outSample);
        }
    }
    else if(couplingMode == INTERLINKED)
    {
        // DBG("Total gain: " + juce::String(totalGain));
        // for(auto r: resonators)
        // {
        //     DBG("Resonator gain: " + juce::String(r->gain));
        //     DBG("Adjusted gain:" + juce::String(r->gain / totalGain));
        // }

        for(int i = 0; i < exciterBlock.getNumSamples(); i++)
        {
            float resonatorOutSamples[NUM_RESONATORS];
            float outSample = 0.0f;
            float feedbackSample =  0.0f;

            for(int j = 0; j < NUM_RESONATORS; j++)
            {
                resonatorOutSamples[j] = resonators[j]->popSample();
                feedbackSample += resonatorOutSamples[j] * (resonators[j]->gain / totalGain);
                outSample += resonatorOutSamples[j] * resonators[j]->gain;
            }

            //apply the bridge filter H(z) = -2.
            //This is a necessary criterion for stability
            feedbackSample = -2.0f * feedbackSample;

            for(int j = 0; j < NUM_RESONATORS; j++)
            {
                resonators[j]->pushSample(feedbackSample + resonatorOutSamples[j] + exciterBlock.getSample(0, i));
            }

            outputBlock.addSample(0, i, outSample);
            outputBlock.addSample(1, i, outSample);
        }

    } else if(couplingMode == CASCADE)
    {
        jassertfalse; //NYI
    }
    else
    {
        DBG("Invalid feedback mode!");
        jassertfalse;
    }
}

void ResonatorBank::setFeedbackMode(CouplingMode newMode)
{
    this->couplingMode = newMode;
}
