//
// Created by Gabriel Soule on 5/1/24.
//

#include "ResonatorSynth.h"
#include "ResonatorVoice.h"
#include "PluginProcessor.h"

#define NUM_VOICES 16

ResonatorSynth::ResonatorSynth(ResonariumProcessor& p) : processor(p)
{
    DBG("Creating Synthesiser");
}

void ResonatorSynth::prepare(const juce::dsp::ProcessSpec& spec)
{
    setCurrentPlaybackSampleRate(spec.sampleRate);
    for (auto* v : voices)
    {
        dynamic_cast<ResonatorVoice*>(v)->prepare(spec);
    }

    for (int i = 0; i < NUM_LFOS; i++)
    {
        monoLFOs[i].setSampleRate(spec.sampleRate);
    }

    for (int i = 0; i < NUM_MSEGS; i++)
    {
        msegData.add(gin::MSEG::Data());
        gin::MSEG mseg(msegData.getReference(i));
        monoMSEGs.add(mseg);
    }
}

void ResonatorSynth::updateParameters()
{
    // Update Mono LFOs
    for (int i = 0; i < NUM_LFOS; i++)
    {
        if (params.lfoParams[i].enabled->isOn())
        {
            gin::LFO::Parameters internalParams;

            float freq = 0;
            if (params.lfoParams[i].sync->getProcValue() > 0.0f)
                freq = 1.0f / gin::NoteDuration::getNoteDurations()[size_t(params.lfoParams[i].beat->getProcValue())].
                    toSeconds(processor.getPlayHead());
            else
                freq = processor.modMatrix.getValue(params.lfoParams[i].rate);

            internalParams.waveShape = (gin::LFO::WaveShape)int(params.lfoParams[i].wave->getProcValue());
            internalParams.frequency = freq;
            internalParams.phase = processor.modMatrix.getValue(params.lfoParams[i].phase);
            internalParams.offset = processor.modMatrix.getValue(params.lfoParams[i].offset);
            internalParams.depth = processor.modMatrix.getValue(params.lfoParams[i].depth);
            internalParams.delay = 0;
            internalParams.fade = 0;

            monoLFOs[i].setParameters(internalParams);
            monoLFOs[i].process(currentBlockSize);
            processor.modMatrix.setMonoValue(processor.modSrcMonoLFO[i], monoLFOs[i].getOutput());
        }
        else
        {
            processor.modMatrix.setMonoValue(processor.modSrcMonoLFO[i], 0);
        }
    }

    for (int i = 0; i < NUM_MSEGS; i++)
    {
        if (params.msegParams[i].enabled->isOn())
        {
            gin::MSEG::Parameters internalParams;

            float freq = 0;
            if (params.msegParams[i].sync->getProcValue() > 0.0f)
                freq = 1.0f / gin::NoteDuration::getNoteDurations()[size_t(params.msegParams[i].beat->getProcValue())].
                    toSeconds(processor.getPlayHead());
            else
                freq = processor.modMatrix.getValue(params.msegParams[i].rate);

            internalParams.frequency = freq;
            internalParams.phase = processor.modMatrix.getValue(params.msegParams[i].phase);
            internalParams.offset = processor.modMatrix.getValue(params.msegParams[i].offset);
            internalParams.depth = processor.modMatrix.getValue(params.msegParams[i].depth);
            internalParams.delay = 0;
            internalParams.fade = 0;

            monoMSEGs.getReference(i).setParameters(internalParams);
            monoLFOs[i].process(currentBlockSize);
            processor.modMatrix.setMonoValue(processor.modSrcMonoMSEG[i], monoMSEGs.getReference(i).getOutput());
        }
        else
        {
            processor.modMatrix.setMonoValue(processor.modSrcMonoMSEG[i], 0);
        }
    }
}

void ResonatorSynth::renderNextSubBlock(juce::AudioBuffer<float>& outputAudio, int startSample, int numSamples)
{
    currentBlockSize = numSamples;
    updateParameters();
    Synthesiser::renderNextSubBlock(outputAudio, startSample, numSamples);
}
