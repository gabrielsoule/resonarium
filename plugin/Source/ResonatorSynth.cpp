//
// Created by Gabriel Soule on 5/1/24.
//

#include "ResonatorSynth.h"
#include "ResonatorVoice.h"
#include "PluginProcessor.h"

ResonatorSynth::ResonatorSynth(ResonariumProcessor& p) : proc(p)
{

}

void ResonatorSynth::setupParameters()
{
    params.setup(proc);

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
}

void ResonatorSynth::updateParameters()
{
    int rawIndex = static_cast<int>(params.soloResonator->getProcValue());
    soloActive = rawIndex > -1.0f;
    if(soloActive)
    {
        soloBankIndex = std::floor(rawIndex / NUM_RESONATORS);
        soloResonatorIndex = rawIndex % NUM_RESONATORS;
        // DBG("Solo active at bank " << soloBankIndex << " resonator " << soloResonatorIndex);
        // DBG(rawIndex);
    }
    for (int i = 0; i < NUM_LFOS; i++)
    {
        if (params.lfoParams[i].enabled->isOn())
        {
            float freq = 0;
            if (params.lfoParams[i].sync->getProcValue() > 0.0f)
                freq = 1.0f / gin::NoteDuration::getNoteDurations()[size_t(params.lfoParams[i].beat->getProcValue())].
                    toSeconds(proc.getPlayHead());
            else
                freq = proc.modMatrix.getValue(params.lfoParams[i].rate);
            monoLFOs[i].updateParameters(proc.modMatrix, freq);
            monoLFOs[i].process(currentBlockSize);
            proc.modMatrix.setMonoValue(proc.modSrcMonoLFO[i], monoLFOs[i].getOutput(0), 0);
            proc.modMatrix.setMonoValue(proc.modSrcMonoLFO[i], monoLFOs[i].getOutput(1), 1);
        }
        else
        {
            proc.modMatrix.setMonoValue(proc.modSrcMonoLFO[i], 0, 0);
            proc.modMatrix.setMonoValue(proc.modSrcMonoLFO[i], 0, 1);
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
                    toSeconds(proc.getPlayHead());
            else
                rate = proc.modMatrix.getValue(params.randomLfoParams[i].rate);
            monoRandomLFOs[i].updateParametersMono(proc.modMatrix, rate);
            monoRandomLFOs[i].process(currentBlockSize);
            proc.modMatrix.setMonoValue(proc.modSrcMonoRND[i], monoRandomLFOs[i].getOutput(0), 0);
            proc.modMatrix.setMonoValue(proc.modSrcMonoRND[i], monoRandomLFOs[i].getOutput(1), 1);
        }
    }

    for (int i = 0; i < NUM_MSEGS; i++)
    {
        if (params.msegParams[i].enabled->isOn())
        {
            float rate = 0;
            if (params.msegParams[i].sync->getProcValue() > 0.0f)
                rate = 1.0f / gin::NoteDuration::getNoteDurations()[size_t(params.msegParams[i].beat->getProcValue())].
                    toSeconds(proc.getPlayHead());
            else
                rate = proc.modMatrix.getValue(params.msegParams[i].rate);
            monoMSEGs.getReference(i).updateParameters(proc.modMatrix, rate);
            monoMSEGs.getReference(i).process(currentBlockSize);
            proc.modMatrix.setMonoValue(proc.modSrcMonoMSEG[i], monoMSEGs.getReference(i).getOutput(0), 0);
            proc.modMatrix.setMonoValue(proc.modSrcMonoMSEG[i], monoMSEGs.getReference(i).getOutput(1), 1);
        }
        else
        {
            proc.modMatrix.setMonoValue(proc.modSrcMonoMSEG[i], 0, 0);
            proc.modMatrix.setMonoValue(proc.modSrcMonoMSEG[i], 0, 1);
        }
    }

    for(int i = 0; i < NUM_MACROS; i++)
    {
        proc.modMatrix.setMonoValue(proc.modSrcMacro[i], proc.modMatrix.getValue(params.macroParams[i]), 0);
        proc.modMatrix.setMonoValue(proc.modSrcMacro[i], proc.modMatrix.getValue(params.macroParams[i]), 1);
    }
}

void ResonatorSynth::renderNextSubBlock(juce::AudioBuffer<float>& outputAudio, int startSample, int numSamples)
{
    currentBlockSize = numSamples;
    updateParameters();
    Synthesiser::renderNextSubBlock(outputAudio, startSample, numSamples);
}

void ResonatorSynth::panic()
{
    //kill voices and reset
    for (auto v : voices)
    {
        stopVoiceFastKill(v, v->getCurrentlyPlayingNote(), false);
    }
}
