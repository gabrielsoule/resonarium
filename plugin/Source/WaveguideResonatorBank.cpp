#include "WaveguideResonatorBank.h"
#include "ResonatorVoice.h"

WaveguideResonatorBank::WaveguideResonatorBank(ResonatorVoice& parentVoice, WaveguideResonatorBankParams params) :
    ResonatorBank(parentVoice), params(params)
{
    //add some resonators to the OwnedArray
    for (int i = 0; i < NUM_WAVEGUIDE_RESONATORS; i++)
    {
        resonators.add(new Resonator(parentVoice, params.resonatorParams[i]));
    }
    waveguideBankIndex = params.index;
    couplingMode = PARALLEL;
}

WaveguideResonatorBank::~WaveguideResonatorBank()
{
    resonators.clear(true);
}

/**
 * Resets the internal state of the resonator bank and its resonators.
 * Use when playing a new note, for example.
 */
void WaveguideResonatorBank::reset()
{
    for (int i = 0; i < NUM_WAVEGUIDE_RESONATORS; i++)
    {
        resonators[i]->reset();
    }
    couplingFilter.reset();
}

void WaveguideResonatorBank::prepare(const juce::dsp::ProcessSpec& spec)
{
    for (int i = 0; i < NUM_WAVEGUIDE_RESONATORS; i++)
    {
        jassert(resonators[i]->params.resonatorIndex == i); //ensure that parameters have been correctly distributed
        resonators[i]->prepare(spec);
    }

    sampleRate = spec.sampleRate;
    couplingFilter.coefficients = juce::dsp::IIR::Coefficients<float>::makeHighPass(spec.sampleRate, 1000.0f, 0.707f);
    couplingFilter.prepare(spec);
    reset();
}

/**
 * Updates all internal parameters based on the state of the VST parameters corresponding to this WaveguideResonatorBank.
 * Child Resonators are also updated.
 * Frequency is passed as an argument since frequency comes from the MIDI note, not the VST parameters.
 */
void WaveguideResonatorBank::updateParameters(float newFrequency)
{
    this->frequency = newFrequency;
    this->couplingMode = static_cast<CouplingMode>(voice.getValue(params.couplingMode));
    for (auto* r : resonators)
    {
        r->updateParameters(newFrequency);
    }
}

void WaveguideResonatorBank::process(juce::dsp::AudioBlock<float>& exciterBlock, juce::dsp::AudioBlock<float>& outputBlock)
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

    if (totalGain == 0.0f) return;

    if (couplingMode == PARALLEL)
    {
        for (int i = 0; i < exciterBlock.getNumSamples(); i++)
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
    else if (couplingMode == INTERLINKED)
    {
        for (int i = 0; i < exciterBlock.getNumSamples(); i++)
        {
            float resonatorOutSamples[NUM_WAVEGUIDE_RESONATORS];
            float outSample = 0.0f;
            float feedbackSample = 0.0f;

            for (int j = 0; j < NUM_WAVEGUIDE_RESONATORS; j++)
            {
                resonatorOutSamples[j] = resonators[j]->popSample();
                feedbackSample += resonatorOutSamples[j] * (resonators[j]->gain / totalGain);
                outSample += resonatorOutSamples[j] * resonators[j]->gain;
            }

            //apply the bridge filter H(z) = -2.
            //This is a necessary criterion for stability
            feedbackSample = -2.0f * feedbackSample;

            for (int j = 0; j < NUM_WAVEGUIDE_RESONATORS; j++)
            {
                resonators[j]->pushSample(feedbackSample + resonatorOutSamples[j] + exciterBlock.getSample(0, i));
            }

            outputBlock.addSample(0, i, outSample);
            outputBlock.addSample(1, i, outSample);
        }
    }
    else if (couplingMode == CASCADE)
    {
        jassertfalse; //NYI
    }
    else
    {
        DBG("Invalid feedback mode!");
        jassertfalse;
    }
}

void WaveguideResonatorBank::setFeedbackMode(CouplingMode newMode)
{
    this->couplingMode = newMode;
}
