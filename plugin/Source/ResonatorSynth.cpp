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
    for(auto* v : voices)
    {
        dynamic_cast<ResonatorVoice*>(v)->prepare(spec);
    }

    for(int i = 0; i < NUM_LFOS; i++)
    {
        monoLFOs[i].setSampleRate(spec.sampleRate);
    }

    DBG("Source ID: " + juce::String(processor.modSrcMonoLFO[0].id));
    DBG("Destination ID: " + juce::String(gin::ModDstId(params.voiceParams.noiseExciterParams[0].filterParams.frequency->getModIndex()).id));
    processor.modMatrix.setModDepth(processor.modSrcMonoLFO[0], gin::ModDstId(params.voiceParams.noiseExciterParams[0].filterParams.frequency->getModIndex()), 1.0f);
    DBG("Size: " + juce::String(params.voiceParams.noiseExciterParams[0].filterParams.frequency->getModMatrix()->getModSources(params.voiceParams.noiseExciterParams[0].filterParams.frequency).size()));
    DBG("Size: " + juce::String(processor.modMatrix.getModSources(params.voiceParams.noiseExciterParams[0].filterParams.frequency).size()));

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
                freq = 1.0f / gin::NoteDuration::getNoteDurations()[size_t (params.lfoParams[i].beat->getProcValue())].toSeconds (processor.getPlayHead());
            else
                freq = processor.modMatrix.getValue (params.lfoParams[i].rate);

            internalParams.waveShape = (gin::LFO::WaveShape) int (params.lfoParams[i].wave->getProcValue());
            internalParams.frequency = freq;
            internalParams.phase     = processor.modMatrix.getValue (params.lfoParams[i].phase);
            internalParams.offset    = processor.modMatrix.getValue (params.lfoParams[i].offset);
            internalParams.depth     = processor.modMatrix.getValue (params.lfoParams[i].depth);
            internalParams.delay     = 0;
            internalParams.fade      = 0;

            monoLFOs[i].setParameters (internalParams);
            monoLFOs[i].process(currentBlockSize);
            processor.modMatrix.setMonoValue (processor.modSrcMonoLFO[i], monoLFOs[i].getOutput());
        }
        else
        {
            processor.modMatrix.setMonoValue (processor.modSrcMonoLFO[i], 0);
        }
    }

}

void ResonatorSynth::renderNextSubBlock(juce::AudioBuffer<float>& outputAudio, int startSample, int numSamples)
{
    currentBlockSize = numSamples;
    updateParameters();
    Synthesiser::renderNextSubBlock(outputAudio, startSample, numSamples);
}
