#include "ResonatorSynth.h"
#include "ResonatorVoice.h"
#include "PluginProcessor.h"

ResonatorSynth::ResonatorSynth(GlobalState& state, SynthParams params) : state(state), params(params), effectChain(params.effectChainParams)
{
    monoMSEGs.clear();
    msegData.clear();

    for (int i = 0; i < NUM_LFOS; i++)
    {
        monoLFOs[i].params = params.lfoParams[i];
    }

    for (int i = 0; i < NUM_RANDOMS; i++)
    {
        monoRandomLFOs[i].params = params.randomLfoParams[i];
    }

    for (int i = 0; i < NUM_MSEGS; i++)
    {
        auto mseg = StereoMSEGWrapper(params.msegParams[i]);
        monoMSEGs.add(mseg);
    }
}
void ResonatorSynth::prepare(const juce::dsp::ProcessSpec& spec)
{
    setCurrentPlaybackSampleRate(spec.sampleRate);

    for (auto* v : voices)
    {
        dynamic_cast<ResonatorVoice*>(v)->prepare(spec);
    }

    for (auto & monoLFO : monoLFOs)
    {
        monoLFO.reset();
        monoLFO.prepare(spec);
    }

    for (auto & monoRandomLFO : monoRandomLFOs)
    {
        monoRandomLFO.reset();
        monoRandomLFO.prepare(spec);
    }

    for (int i = 0; i < NUM_MSEGS; i++)
    {
        monoMSEGs.getReference(i).prepare(spec);
    }
    effectChain.reset();
    effectChain.prepare(spec);
    updateParameters();
    state.modMatrix.snapParams();

}

void ResonatorSynth::updateParameters()
{
    int rawIndex = static_cast<int>(params.soloResonator->getProcValue());
    state.soloActive = rawIndex > -1.0f;
    if(state.soloActive)
    {
        state.soloBankIndex = std::floor(rawIndex / NUM_RESONATORS);
        state.soloResonatorIndex = rawIndex % NUM_RESONATORS;
    }
    state.polyFX = params.globalParams.polyEffectChain->isOn();
    for (int i = 0; i < NUM_LFOS; i++)
    {
        if (params.lfoParams[i].enabled->isOn())
        {
            float freq = 0;
            if (params.lfoParams[i].sync->getProcValue() > 0.0f)
                freq = 1.0f / gin::NoteDuration::getNoteDurations()[size_t(params.lfoParams[i].beat->getProcValue())].
                    toSeconds(state.playHead);
            else
                freq = state.modMatrix.getValue(params.lfoParams[i].rate);
            monoLFOs[i].updateParameters(state.modMatrix, freq);
            monoLFOs[i].process(currentBlockSize);
            state.modMatrix.setMonoValue(state.modSrcMonoLFO[i], monoLFOs[i].getOutput(0), 0);
            state.modMatrix.setMonoValue(state.modSrcMonoLFO[i], monoLFOs[i].getOutput(1), 1);
        }
        else
        {
            state.modMatrix.setMonoValue(state.modSrcMonoLFO[i], 0, 0);
            state.modMatrix.setMonoValue(state.modSrcMonoLFO[i], 0, 1);
        }
    }

    for (int i = 0; i < NUM_RANDOMS; i++)
    {
        if (params.randomLfoParams[i].enabled->isOn())
        {
            float rate = 0;
            if (params.randomLfoParams[i].sync->getProcValue() > 0.0f)
                rate = 1.0f / gin::NoteDuration::getNoteDurations()[size_t(
                        params.randomLfoParams[i].beat->getProcValue())].
                    toSeconds(state.playHead);
            else
                rate = state.modMatrix.getValue(params.randomLfoParams[i].rate);
            monoRandomLFOs[i].updateParametersMono(state.modMatrix, rate);
            monoRandomLFOs[i].process(currentBlockSize);
            state.modMatrix.setMonoValue(state.modSrcMonoRND[i], monoRandomLFOs[i].getOutput(0), 0);
            state.modMatrix.setMonoValue(state.modSrcMonoRND[i], monoRandomLFOs[i].getOutput(1), 1);
        }
    }

    for (int i = 0; i < NUM_MSEGS; i++)
    {
        if (params.msegParams[i].enabled->isOn())
        {
            float rate = 0;
            if (params.msegParams[i].sync->getProcValue() > 0.0f)
                rate = 1.0f / gin::NoteDuration::getNoteDurations()[size_t(params.msegParams[i].beat->getProcValue())].
                    toSeconds(state.playHead);
            else
                rate = state.modMatrix.getValue(params.msegParams[i].rate);
            monoMSEGs.getReference(i).updateParameters(state.modMatrix, rate);
            monoMSEGs.getReference(i).process(currentBlockSize);
            state.modMatrix.setMonoValue(state.modSrcMonoMSEG[i], monoMSEGs.getReference(i).getOutput(0), 0);
            state.modMatrix.setMonoValue(state.modSrcMonoMSEG[i], monoMSEGs.getReference(i).getOutput(1), 1);
        }
        else
        {
            state.modMatrix.setMonoValue(state.modSrcMonoMSEG[i], 0, 0);
            state.modMatrix.setMonoValue(state.modSrcMonoMSEG[i], 0, 1);
        }
    }

    for(int i = 0; i < NUM_MACROS; i++)
    {
        state.modMatrix.setMonoValue(state.modSrcMacro[i], state.modMatrix.getValue(params.macroParams[i]), 0);
        state.modMatrix.setMonoValue(state.modSrcMacro[i], state.modMatrix.getValue(params.macroParams[i]), 1);
    }

    if (! state.polyFX)
    {
        effectChain.updateParameters(state.modMatrix, state.playHead);
    }
}

void ResonatorSynth::renderNextSubBlock(juce::AudioBuffer<float>& outputAudio, int startSample, int numSamples)
{
    currentBlockSize = numSamples;
    updateParameters();
    Synthesiser::renderNextSubBlock(outputAudio, startSample, numSamples);
    juce::dsp::AudioBlock<float> block = juce::dsp::AudioBlock<float>(outputAudio).getSubBlock(startSample, numSamples);

    if (! state.polyFX )
    {
        effectChain.process(block);
    }
}

void ResonatorSynth::panic()
{
    //kill voices and reset
    for (auto v : voices)
    {
        stopVoiceFastKill(v, v->getCurrentlyPlayingNote(), false);
    }
}
