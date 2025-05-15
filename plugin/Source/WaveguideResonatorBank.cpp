#include "WaveguideResonatorBank.h"
#include "ResonatorVoice.h"

WaveguideResonatorBank::WaveguideResonatorBank(GlobalState& state, ResonatorVoice& parentVoice,
                                               WaveguideResonatorBankParams params) :
    state(state),
    voice(parentVoice),
    params(params)
{
    dcBlockerCoefficients =
        new juce::dsp::IIR::Coefficients<float>(1, -1, 1, -0.995f);
    for (int i = 0; i < NUM_RESONATORS; i++)
    {
        resonators.add(new StereoResonator(parentVoice, params.resonatorParams[i]));
        dcBlockersL[i] = juce::dsp::IIR::Filter<float>(dcBlockerCoefficients);
        dcBlockersR[i] = juce::dsp::IIR::Filter<float>(dcBlockerCoefficients);
        dcBlockersL[i].coefficients = dcBlockerCoefficients;
        dcBlockersR[i].coefficients = dcBlockerCoefficients;
    }
    index = params.index;
    couplingMode = PARALLEL;
    frequency = -1;
    sampleRate = -1;
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
    for (int i = 0; i < NUM_RESONATORS; i++)
    {
        resonators[i]->reset();
        dcBlockersL[i].reset();
        dcBlockersR[i].reset();
    }
    cascadeFilterL.reset();
    cascadeFilterR.reset();
    // testInterlinkedFilterL.reset();
    // testInterlinkedFilterR.reset();

    for (int i = 0; i < NUM_RESONATORS; ++i)
    {
        crossoverL[i].reset();
        crossoverR[i].reset();
    }
}

void WaveguideResonatorBank::prepare(const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate;
    cascadeFilterL.prepare(spec);
    cascadeFilterR.prepare(spec);
    testInterlinkedFilterL.prepare(spec);
    testInterlinkedFilterR.prepare(spec);

    // prepare complementary Linkwitz–Riley crossover -------------------------
    // Initialize cutoff from parameter
    couplingCutoffHz = voice.getValue(params.couplingCutoff);
    
    // Ensure cutoff doesn't exceed half the sample rate (Nyquist limit)
    float nyquistLimit = sampleRate * 0.5f - 50;
        couplingCutoffHz = nyquistLimit;
        
    for (int i = 0; i < NUM_RESONATORS; ++i)
    {
        crossoverL[i].reset();
        crossoverR[i].reset();
        crossoverL[i].prepare(spec);
        crossoverR[i].prepare(spec);
        crossoverL[i].setCutoffFrequency(couplingCutoffHz);
        crossoverR[i].setCutoffFrequency(couplingCutoffHz);
    }

    for (int i = 0; i < NUM_RESONATORS; i++)
    {
        jassert(resonators[i]->params.resonatorIndex == i); //ensure that parameters have been correctly distributed
        resonators[i]->prepare(spec);
        dcBlockersL[i].coefficients = dcBlockerCoefficients;
        dcBlockersR[i].coefficients = dcBlockerCoefficients;
        dcBlockersL[i].reset();
        dcBlockersR[i].reset();
        dcBlockersL[i].prepare(spec);
        dcBlockersR[i].prepare(spec);
    }

    testInterlinkedFilterL.updateParameters(1000, 1 / std::sqrt(2), 1, false);
    testInterlinkedFilterR.updateParameters(1000, 1 / std::sqrt(2), 1, false);


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
    frequency = newFrequency;
    couplingMode = static_cast<CouplingMode>(voice.getValue(params.couplingMode));
    previousResonatorBankMix = voice.getValue(params.inputMix);
    exciterMix = 1 - voice.getValue(params.inputMix);
    previousResonatorBankMix *= 0.5f; //scale down a little to taste
    inputGain = voice.getValue(params.inputGain);
    outputGain = voice.getValue(params.outputGain);
    
    // Update coupling cutoff frequency ONLY if in COUPLED_FLTR mode
    if (couplingMode == COUPLED_FLTR)
    {
        float newCutoff = voice.getValue(params.couplingCutoff);
        
        // Ensure cutoff doesn't exceed half the sample rate (Nyquist limit)
        float nyquistLimit = sampleRate * 0.5f;
        if (newCutoff > nyquistLimit)
            newCutoff = nyquistLimit;
            
        if (newCutoff != couplingCutoffHz)
        {
            couplingCutoffHz = newCutoff;
            for (int i = 0; i < NUM_RESONATORS; ++i)
            {
                crossoverL[i].setCutoffFrequency(couplingCutoffHz);
                crossoverR[i].setCutoffFrequency(couplingCutoffHz);
            }
        }
    }
    
    for (auto* r : resonators)
    {
        r->updateParameters(newFrequency, numSamples);
    }
}

//TODO: with the addition of stereo resonators, we should rewrite this whole function to support stereo processing without all the code duplication
/**
 * READS FROM the exciter block and CONSUMES the previous resonator bank block.
 * The output is (over) written to the previous resonator bank block.
 * @param exciterBlock
 * @param previousResonatorBankBlock
 */
void WaveguideResonatorBank::process(juce::dsp::AudioBlock<float>& exciterBlock,
                                     juce::dsp::AudioBlock<float>& previousResonatorBankBlock)
{
    //reminder: the outputBlock may already have samples inside...
    jassert(exciterBlock.getNumSamples() == previousResonatorBankBlock.getNumSamples());
    jassert(exciterBlock.getNumChannels() == previousResonatorBankBlock.getNumChannels());

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
            for (int j = 0; j < NUM_RESONATORS; j++)
            {
                if (resonators[j]->enabled)
                {
                    const float inSampleL = (exciterBlock.getSample(0, i) * exciterMix + previousResonatorBankBlock.
                        getSample(0, i) * previousResonatorBankMix) * inputGain;
                    const float inSampleR = (exciterBlock.getSample(1, i) * exciterMix + previousResonatorBankBlock.
                        getSample(1, i) * previousResonatorBankMix) * inputGain;
                    const float soloOutSampleL = resonators[j]->left.postProcess(
                        resonators[j]->left.processSample(inSampleL)) * resonators[j]->left.gain;
                    const float soloOutSampleR = resonators[j]->right.postProcess(
                        resonators[j]->right.processSample(inSampleR)) * resonators[j]->right.gain;
                    outSampleL += soloOutSampleL;
                    outSampleR += soloOutSampleR;

                    if (state.soloActive &&
                        state.soloBankIndex == index &&
                        state.soloResonatorIndex == resonators[j]->resonatorIndex)
                    {
                        voice.soloBuffer.setSample(0, voice.currentBlockStartSample + i, soloOutSampleL);
                        voice.soloBuffer.setSample(1, voice.currentBlockStartSample + i, soloOutSampleR);
                    }
                }
            }
            outSampleL = (outSampleL / totalGainL) * outputGain;
            outSampleR = (outSampleR / totalGainR) * outputGain;


            previousResonatorBankBlock.setSample(0, i, outSampleL);
            previousResonatorBankBlock.setSample(1, i, outSampleR);
        }
    }
    else if (couplingMode == COUPLED)
    {
        for (int i = 0; i < exciterBlock.getNumSamples(); i++)
        {
            float resonatorOutSamplesL[NUM_RESONATORS];
            float resonatorOutSamplesR[NUM_RESONATORS];
            float outSampleL = 0.0f;
            float outSampleR = 0.0f;
            float feedbackSampleL = 0.0f;
            float feedbackSampleR = 0.0f;

            for (int j = 0; j < NUM_RESONATORS; j++)
            {
                resonatorOutSamplesL[j] = resonators[j]->popSample(0);
                resonatorOutSamplesR[j] = resonators[j]->popSample(1);
                feedbackSampleL += resonatorOutSamplesL[j] * (resonators[j]->resonators[0].gain / totalGainL);
                feedbackSampleR += resonatorOutSamplesR[j] * (resonators[j]->resonators[1].gain / totalGainR);
                outSampleL += resonators[j]->postProcess(resonatorOutSamplesL[j], 0) * resonators[j]->resonators[0].
                    gain;
                outSampleR += resonators[j]->postProcess(resonatorOutSamplesL[j], 1) * resonators[j]->resonators[1].
                    gain;
            }

            if (state.soloActive && state.soloBankIndex == index)
            {
                voice.soloBuffer.setSample(0, voice.currentBlockStartSample + i,
                                           resonatorOutSamplesL[state.soloResonatorIndex]);
                voice.soloBuffer.setSample(1, voice.currentBlockStartSample + i,
                                           resonatorOutSamplesR[state.soloResonatorIndex]);
            }

            //apply the bridge filter H(z) = -2 / totalGain. This is a necessary criterion for stability
            feedbackSampleL = -2.0f * feedbackSampleL;
            feedbackSampleR = -2.0f * feedbackSampleR;
            // feedbackSampleL = testInterlinkedFilterL.processSample(0, feedbackSampleL);
            // feedbackSampleR = testInterlinkedFilterR.processSample(0, feedbackSampleR);

            for (int j = 0; j < NUM_RESONATORS; j++)
            {
                const float inSampleL = (exciterBlock.getSample(0, i) * exciterMix + previousResonatorBankBlock.
                    getSample(0, i) * previousResonatorBankMix) * inputGain;
                const float inSampleR = (exciterBlock.getSample(1, i) * exciterMix + previousResonatorBankBlock.
                    getSample(1, i) * previousResonatorBankMix) * inputGain;
                resonators[j]->
                    pushSample((feedbackSampleL + resonatorOutSamplesL[j]) * -1 + inSampleL, 0);
                resonators[j]->
                    pushSample((feedbackSampleR + resonatorOutSamplesR[j]) * -1 + inSampleR, 1);
            }

            previousResonatorBankBlock.setSample(0, i, outSampleL * outputGain);
            previousResonatorBankBlock.setSample(1, i, outSampleR * outputGain);
        }
    }
    else if (couplingMode == COUPLED_FLTR)
    {
        for (int i = 0; i < exciterBlock.getNumSamples(); ++i)
        {
            float delayedOutL[NUM_RESONATORS]; // lp + hp  (time‑aligned)
            float delayedOutR[NUM_RESONATORS];

            float outSampleL = 0.0f, outSampleR = 0.0f;
            float hpSumL = 0.0f, hpSumR = 0.0f;

            /* -------------------------------------------------------------
               pop each string, split into LP / HP, accumulate high‑pass sum
               ------------------------------------------------------------- */
            for (int j = 0; j < NUM_RESONATORS; ++j)
            {
                const float aL = resonators[j]->popSample(0);
                const float aR = resonators[j]->popSample(1);

                float lpL, hpL;
                crossoverL[j].processSample(0, aL, lpL, hpL);

                float lpR, hpR;
                crossoverR[j].processSample(0, aR, lpR, hpR);

                delayedOutL[j] = lpL + hpL; // exactly aL, but delayed by the filter
                delayedOutR[j] = lpR + hpR;

                hpSumL += hpL * (resonators[j]->resonators[0].gain / totalGainL);
                hpSumR += hpR * (resonators[j]->resonators[1].gain / totalGainR);

                outSampleL += resonators[j]->postProcess(aL, 0)
                    * resonators[j]->resonators[0].gain;
                outSampleR += resonators[j]->postProcess(aR, 1)
                    * resonators[j]->resonators[1].gain;
            }

            /* -------------------- Householder reflection for HP band -------------------- */
            const float feedbackL = -2.0f * hpSumL;
            const float feedbackR = -2.0f * hpSumR;

            /* -------------------- push back into every string --------------------------- */
            for (int j = 0; j < NUM_RESONATORS; ++j)
            {
                const float inL = (exciterBlock.getSample(0, i) * exciterMix
                    + previousResonatorBankBlock.getSample(0, i) * previousResonatorBankMix) * inputGain;

                const float inR = (exciterBlock.getSample(1, i) * exciterMix
                    + previousResonatorBankBlock.getSample(1, i) * previousResonatorBankMix) * inputGain;

                const float nextL = delayedOutL[j] + feedbackL + inL;
                const float nextR = delayedOutR[j] + feedbackR + inR;

                resonators[j]->pushSample(nextL * -1, 0);
                resonators[j]->pushSample(nextR * -1, 1);
            }

            previousResonatorBankBlock.setSample(0, i, outSampleL * outputGain);
            previousResonatorBankBlock.setSample(1, i, outSampleR * outputGain);
        }
    }
    else if (couplingMode == CASCADE)
    {
        const float cascadeAmountL = voice.getValue(params.cascadeLevel, 0);
        const float cascadeAmountR = voice.getValue(params.cascadeLevel, 1);

        const float newCutoffL = voice.getValue(params.cascadeFilterCutoff, 0);
        const float newResonanceL = voice.getValue(params.cascadeFilterResonance, 0);
        const float newModeL = voice.getValue(params.cascadeFilterMode, 0);
        cascadeFilterL.updateParameters(newCutoffL, newResonanceL, newModeL);

        const float newCutoffR = voice.getValue(params.cascadeFilterCutoff, 1);
        const float newResonanceR = voice.getValue(params.cascadeFilterResonance, 1);
        const float newModeR = voice.getValue(params.cascadeFilterMode, 1);
        cascadeFilterR.updateParameters(newCutoffR, newResonanceR, newModeR);

        for (int i = 0; i < exciterBlock.getNumSamples(); i++)
        {
            float previousResonatorSampleL = 0.0f;
            float previousResonatorSampleR = 0.0f;
            float outSampleL = 0.0f;
            float outSampleR = 0.0f;
            for (int j = 0; j < NUM_RESONATORS; j++)
            {
                if (resonators[j]->enabled)
                {
                    const float resonatorOutSampleL = resonators[j]->popSample(0);
                    const float resonatorOutSampleR = resonators[j]->popSample(1);
                    const float processedFwdSampleL = cascadeFilterL.processSample(
                        j, dcBlockersL[j].processSample(previousResonatorSampleL) * cascadeAmountL);
                    const float processedFwdSampleR = cascadeFilterR.processSample(
                        j, dcBlockersR[j].processSample(previousResonatorSampleR) * cascadeAmountR);
                    const float inSampleL = (exciterBlock.getSample(0, i) * exciterMix + previousResonatorBankBlock.
                        getSample(0, i) * previousResonatorBankMix) * inputGain;
                    const float inSampleR = (exciterBlock.getSample(1, i) * exciterMix + previousResonatorBankBlock.
                        getSample(1, i) * previousResonatorBankMix) * inputGain;
                    resonators[j]->pushSample(resonatorOutSampleL + processedFwdSampleL + inSampleL, 0);
                    resonators[j]->pushSample(resonatorOutSampleR + processedFwdSampleR + inSampleR, 1);
                    previousResonatorSampleL = resonatorOutSampleL;
                    previousResonatorSampleR = resonatorOutSampleR;
                    const float soloOutSampleL =
                        resonators[j]->postProcess(resonatorOutSampleL, 0) * resonators[j]->resonators[0].gain;
                    const float soloOutSampleR =
                        resonators[j]->postProcess(resonatorOutSampleR, 1) * resonators[j]->resonators[1].gain;
                    outSampleR += soloOutSampleR;
                    outSampleL += soloOutSampleL;

                    if (state.soloActive &&
                        state.soloBankIndex == index &&
                        state.soloResonatorIndex == resonators[j]->resonatorIndex)
                    {
                        voice.soloBuffer.setSample(0, voice.currentBlockStartSample + i, soloOutSampleL);
                        voice.soloBuffer.setSample(1, voice.currentBlockStartSample + i, soloOutSampleR);
                    }
                }
            }

            previousResonatorBankBlock.setSample(0, i, outSampleL * outputGain);
            previousResonatorBankBlock.setSample(1, i, outSampleR * outputGain);
        }
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
