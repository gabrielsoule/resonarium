#include "WaveguideResonatorBank.h"
#include "ResonatorVoice.h"

WaveguideResonatorBank::WaveguideResonatorBank(ResonatorVoice& parentVoice, WaveguideResonatorBankParams params) :
    ResonatorBank(parentVoice), params(params)
{
    dcBlockerCoefficients =
        new juce::dsp::IIR::Coefficients<float>(1, -1, 1, -0.995f);
    for (int i = 0; i < NUM_WAVEGUIDE_RESONATORS; i++)
    {
        resonators.add(new StereoResonator(parentVoice, params.resonatorParams[i]));
        dcBlockersL[i] = juce::dsp::IIR::Filter<float>(dcBlockerCoefficients);
        dcBlockersR[i] = juce::dsp::IIR::Filter<float>(dcBlockerCoefficients);
        dcBlockersL[i].coefficients = dcBlockerCoefficients;
        dcBlockersR[i].coefficients = dcBlockerCoefficients;
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
        dcBlockersL[i].reset();
        dcBlockersR[i].reset();
    }
    cascadeFilterL.reset();
    cascadeFilterR.reset();
    couplingFilter.reset();
    couplingFilterFIR.reset();
}

void WaveguideResonatorBank::prepare(const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate;
    couplingCoefficientsFIR = juce::dsp::FilterDesign<float>::designFIRLowpassWindowMethod(
        10000, sampleRate, 21, juce::dsp::WindowingFunction<float>::hamming);
    // couplingFilterFIR.prepare(spec);
    couplingFilterFIR.coefficients = couplingCoefficientsFIR;
    cascadeFilterL.prepare(spec);
    cascadeFilterR.prepare(spec);

    for (int i = 0; i < NUM_WAVEGUIDE_RESONATORS; i++)
    {
        jassert(resonators[i]->params.resonatorIndex == i); //ensure that parameters have been correctly distributed
        resonators[i]->prepare(spec);
        firDelays[i].prepare(spec);
        firDelays[i].setMaximumDelayInSamples(100);
        firDelays[i].setDelay(10);
        firDelays[i].reset();
        dcBlockersL[i].coefficients = dcBlockerCoefficients;
        dcBlockersR[i].coefficients = dcBlockerCoefficients;
        dcBlockersL[i].reset();
        dcBlockersR[i].reset();
        dcBlockersL[i].prepare(spec);
        dcBlockersR[i].prepare(spec);
    }


    reset();
    updateParameters(440.0f, 0);
}

/**
 * Updates all internal parameters based on the state of the VST parameters corresponding to this WaveguideResonatorBank.
 * Child Resonators are also updated.
 * Frequency is passed as an argument since frequency comes from the MIDI note, not the VST parameters.
 */
void WaveguideResonatorBank::updateParameters(float newFrequency, int numSamples)
{
    this->frequency = newFrequency;
    this->couplingMode = static_cast<CouplingMode>(voice.getValue(params.couplingMode));
    for (auto* r : resonators)
    {
        r->updateParameters(newFrequency, numSamples);
    }
}

//TODO: with the addition of stereo resonators, we should rewrite this whole function to support stereo processing without all the code duplication
void WaveguideResonatorBank::process(juce::dsp::AudioBlock<float>& exciterBlock,
                                     juce::dsp::AudioBlock<float>& outputBlock)
{
    //reminder: the outputBlock may already have samples inside...
    jassert(exciterBlock.getNumSamples() == outputBlock.getNumSamples());
    jassert(exciterBlock.getNumChannels() == outputBlock.getNumChannels());

    //compute the total gain so that the output can be normalized,
    //used by all resonator bank processing modes
    float totalGainL = 0.0f;
    float totalGainR = 0.0f;
    for (const auto* r : resonators)
    {
        totalGainL = totalGainL + r->resonators[0].gain;
        totalGainR = totalGainR + r->resonators[1].gain;
    }

    if (totalGainL == 0.0f || totalGainR == 0.0f) return;

    if (couplingMode == PARALLEL)
    {
        for (int i = 0; i < exciterBlock.getNumSamples(); i++)
        {
            float outSampleL = 0.0f;
            float outSampleR = 0.0f;
            for (auto* r : resonators)
            {
                if (r->enabled)
                {
                    outSampleL += r->postProcess(r->processSample(exciterBlock.getSample(0, i), 0), 0) * r->left.gain;
                    outSampleR += r->postProcess(r->processSample(exciterBlock.getSample(1, i), 1), 1) * r->right.gain;
                }
            }
            outSampleL = outSampleL / totalGainL;
            outSampleR = outSampleR / totalGainR;
            outputBlock.addSample(0, i, outSampleL);
            outputBlock.addSample(1, i, outSampleR);
        }
    }
    else if (couplingMode == INTERLINKED)
    {
        for (int i = 0; i < exciterBlock.getNumSamples(); i++)
        {
            float resonatorOutSamplesL[NUM_WAVEGUIDE_RESONATORS];
            float resonatorOutSamplesR[NUM_WAVEGUIDE_RESONATORS];
            float outSampleL = 0.0f;
            float outSampleR = 0.0f;
            float feedbackSampleL = 0.0f;
            float feedbackSampleR = 0.0f;

            for (int j = 0; j < NUM_WAVEGUIDE_RESONATORS; j++)
            {
                resonatorOutSamplesL[j] = resonators[j]->popSample(0);
                resonatorOutSamplesR[j] = resonators[j]->popSample(1);
                feedbackSampleL += resonatorOutSamplesL[j] * (resonators[j]->resonators[0].gain / totalGainL);
                feedbackSampleR += resonatorOutSamplesR[j] * (resonators[j]->resonators[1].gain / totalGainR);
                outSampleL += resonators[j]->postProcess(resonatorOutSamplesL[j], 0) * resonators[j]->resonators[0].gain;
                outSampleR += resonators[j]->postProcess(resonatorOutSamplesL[j], 1) * resonators[j]->resonators[1].gain;
            }

            //apply the bridge filter H(z) = -2.
            //This is a necessary criterion for stability
            feedbackSampleL = -2.0f * feedbackSampleL;
            feedbackSampleR = -2.0f * feedbackSampleR;

            for (int j = 0; j < NUM_WAVEGUIDE_RESONATORS; j++)
            {
                resonators[j]->
                    pushSample((feedbackSampleL + resonatorOutSamplesL[j]) * -1 + exciterBlock.getSample(0, i), 0);
                resonators[j]->
                    pushSample((feedbackSampleR + resonatorOutSamplesR[j]) * -1 + exciterBlock.getSample(1, i), 1);
            }

            outputBlock.addSample(0, i, outSampleL);
            outputBlock.addSample(1, i, outSampleR);
        }
    }
    else if (couplingMode == CASCADE)
    {
        float cascadeAmount = voice.getValue(params.cascadeLevel);

        float newCutoffL = voice.getValue(params.cascadeFilterCutoff, 0);
        float newResonanceL = voice.getValue(params.cascadeFilterResonance, 0);
        float newModeL = voice.getValue(params.cascadeFilterMode, 0);
        if(newCutoffL != cascadeFilterCutoffL || newResonanceL != cascadeFilterResonanceL || newModeL != cascadeFilterModeL)
        {
            cascadeFilterCutoffL = newCutoffL;
            cascadeFilterResonanceL = newResonanceL;
            cascadeFilterModeL = newModeL;
            cascadeFilterL.setCutoffFrequency<false>(newCutoffL);
            cascadeFilterL.setQValue<false>(newResonanceL);
            cascadeFilterL.setMode(newModeL);
            cascadeFilterL.update();
            cascadeFilterNormalizationScalarL = 1.0f / cascadeFilterL.getMultiModeMaxGain();
        }

        float newCutoffR = voice.getValue(params.cascadeFilterCutoff, 1);
        float newResonanceR = voice.getValue(params.cascadeFilterResonance, 1);
        float newModeR = voice.getValue(params.cascadeFilterMode, 1);
        if(newCutoffR != cascadeFilterCutoffR || newResonanceR != cascadeFilterResonanceR || newModeR != cascadeFilterModeR)
        {
            cascadeFilterCutoffR = newCutoffR;
            cascadeFilterResonanceR = newResonanceR;
            cascadeFilterModeR = newModeR;
            cascadeFilterR.setCutoffFrequency<false>(newCutoffR);
            cascadeFilterR.setQValue<false>(newResonanceR);
            cascadeFilterR.setMode(newModeR);
            cascadeFilterR.update();
            cascadeFilterNormalizationScalarR = 1.0f / cascadeFilterR.getMultiModeMaxGain();
        }

        for(int i = 0; i < exciterBlock.getNumSamples(); i++)
        {
            float previousResonatorSampleL = 0.0f;
            float previousResonatorSampleR = 0.0f;
            float outSampleL = 0.0f;
            float outSampleR = 0.0f;
            for(int j = 0; j < NUM_WAVEGUIDE_RESONATORS; j++)
            {
                if(resonators[j]->enabled)
                {
                    const float resonatorOutSampleL = resonators[j]->popSample(0);
                    const float resonatorOutSampleR = resonators[j]->popSample(1);
                    const float processedFwdSampleL = cascadeFilterL.processSample(j, dcBlockersL[j].processSample(previousResonatorSampleL) * cascadeAmount);
                    const float processedFwdSampleR = cascadeFilterR.processSample(j, dcBlockersR[j].processSample(previousResonatorSampleR) * cascadeAmount);
                    resonators[j]->pushSample(resonatorOutSampleL + processedFwdSampleL + exciterBlock.getSample(0, i), 0);
                    resonators[j]->pushSample(resonatorOutSampleR + processedFwdSampleR + exciterBlock.getSample(1, i), 1);
                    previousResonatorSampleL = resonatorOutSampleL;
                    previousResonatorSampleR = resonatorOutSampleR;
                    outSampleL += resonators[j]->postProcess(resonatorOutSampleL, 0) * resonators[j]->resonators[0].gain;
                    outSampleR += resonators[j]->postProcess(resonatorOutSampleR, 1) * resonators[j]->resonators[1].gain;
                }
            }

            outputBlock.addSample(0, i, outSampleL);
            outputBlock.addSample(1, i, outSampleR);
        }


        // const float processedFwdSampleL = dcBlockersL[j].processSample(resonatorOutSampleL) * cascadeAmount;
        // const float processedFwdSampleR = dcBlockersR[j].processSample(resonatorOutSampleR) * cascadeAmount;
        // resonators[j]->pushSample(resonatorOutSampleL + processedFwdSampleL + exciterBlock.getSample(0, i), 0);
        // resonators[j]->pushSample(resonatorOutSampleR + processedFwdSampleR + exciterBlock.getSample(1, i), 1);

        // for (int i = 0; i < exciterBlock.getNumSamples(); i++)
        // {
        //     float resonatorOutSamples[NUM_WAVEGUIDE_RESONATORS];
        //     float outSample = 0.0f;
        //     float feedbackSample = 0.0f;
        //
        //     for (int j = 0; j < NUM_WAVEGUIDE_RESONATORS; j++)
        //     {
        //         resonatorOutSamples[j] = resonators[j]->popSample();
        //         feedbackSample += resonatorOutSamples[j] * (resonators[j]->gain / totalGain);
        //         outSample += resonatorOutSamples[j] * resonators[j]->gain;
        //     }
        //
        //     //apply the bridge filter H(z) = -2
        //     //This is a necessary criterion for stability
        //     feedbackSample = -2.0f * feedbackSample;
        //     feedbackSample = couplingFilterFIR.processSample(feedbackSample);
        //
        //     for (int j = 0; j < NUM_WAVEGUIDE_RESONATORS; j++)
        //     {
        //         float resonatorOutSample = firDelays[j].popSample(0);
        //         firDelays[j].pushSample(0, resonatorOutSamples[j]);
        //         resonators[j]->pushSample(feedbackSample + resonatorOutSample + exciterBlock.getSample(0, i));
        //     }
        //
        //     outputBlock.addSample(0, i, outSample);
        //     outputBlock.addSample(1, i, outSample);
        // }
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
