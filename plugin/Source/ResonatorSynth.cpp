//
// Created by Gabriel Soule on 5/1/24.
//

#include "ResonatorSynth.h"
#include "ResonatorVoice.h"
#include "PluginProcessor.h"

#define NUM_VOICES 16

ResonatorSynth::ResonatorSynth(ResonariumProcessor& p) : proc(p)
{
    params.setup(proc);

    monoMSEGs.clear();
    msegData.clear();

    for(int i = 0; i < NUM_LFOS; i++)
    {
        monoLFOs[i].params = params.lfoParams[i];
    }

    for(int i = 0; i < NUM_RANDOMS; i++)
    {
        monoRandomLFOs[i].params = params.randomLfoParams[i];
    }

    for(int i = 0; i < NUM_MSEGS; i++)
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

    for (int i = 0; i < NUM_LFOS; i++)
    {
        monoLFOs[i].reset();
        monoLFOs[i].prepare(spec);
    }

    for(int i = 0; i < NUM_RANDOMS; i++)
    {
        monoRandomLFOs[i].reset();
        monoRandomLFOs[i].prepare(spec);
    }

    for(int i = 0; i < NUM_MSEGS; i++)
    {
        monoMSEGs.getReference(i).prepare(spec);
    }

    chowdsp::SVFMultiMode<float, 2, true> testFilter;
    testFilter.prepare(spec);
    DBG("Testing with parameters " << testFilter.getCutoffFrequency() << " " << testFilter.getQValue());
    DBG("Peak gain " << testFilter.getPeakGain());
    DBG("Setting mode to 0.3, frequency to 2000, and Q to 1.8");
    testFilter.updateParameters(2000, 1.8, 0.3);
    DBG("Testing with parameters " << testFilter.getCutoffFrequency() << " " << testFilter.getQValue());
    DBG("Peak gain " << testFilter.getPeakGain());
    DBG("Setting mode to 0.5, frequency to 1000, and Q to 1.2");
    testFilter.updateParameters(1000, 1.2, 0.5);
    DBG("Testing with parameters " << testFilter.getCutoffFrequency() << " " << testFilter.getQValue());
    DBG("Peak gain " << testFilter.getPeakGain());
    DBG("Setting mode to 0.7, frequency to 500, and Q to 0.8");
    testFilter.updateParameters(500, 0.8, 0.7);
    DBG("Testing with parameters " << testFilter.getCutoffFrequency() << " " << testFilter.getQValue());
    DBG("Peak gain " << testFilter.getPeakGain());
    testFilter.updateParameters(400, 1, 0);
    DBG("Setting mode to 0, frequency to 400, and Q to 1");
    DBG("Testing with parameters " << testFilter.getCutoffFrequency() << " " << testFilter.getQValue());
    DBG("Peak gain " << testFilter.getPeakGain());

}

void ResonatorSynth::updateParameters()
{
    // Update Mono LFOs
    // for (int i = 0; i < NUM_LFOS; i++)
    // {
    //     if (params.lfoParams[i].enabled->isOn())
    //     {
    //         gin::LFO::Parameters internalParams;
    //
    //         float freq = 0;
    //         if (params.lfoParams[i].sync->getProcValue() > 0.0f)
    //             freq = 1.0f / gin::NoteDuration::getNoteDurations()[size_t(params.lfoParams[i].beat->getProcValue())].
    //                 toSeconds(proc.getPlayHead());
    //         else
    //             freq = proc.modMatrix.getValue(params.lfoParams[i].rate);
    //
    //         internalParams.waveShape = (gin::LFO::WaveShape)int(params.lfoParams[i].wave->getProcValue());
    //         internalParams.frequency = freq;
    //         internalParams.phase = proc.modMatrix.getValue(params.lfoParams[i].phase);
    //         internalParams.offset = proc.modMatrix.getValue(params.lfoParams[i].offset);
    //         internalParams.depth = proc.modMatrix.getValue(params.lfoParams[i].depth);
    //         internalParams.delay = 0;
    //         internalParams.fade = 0;
    //
    //         monoLFOs[i].setParameters(internalParams);
    //         monoLFOs[i].process(currentBlockSize);
    //         proc.modMatrix.setMonoValue(proc.modSrcMonoLFO[i], monoLFOs[i].getOutput());
    //     }
    //     else
    //     {
    //         proc.modMatrix.setMonoValue(proc.modSrcMonoLFO[i], 0);
    //     }
    // }

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

    for(int i = 0; i < NUM_RANDOMS; i++)
    {
        if (params.randomLfoParams[i].enabled->isOn())
        {
            float rate = 0;
            if (params.randomLfoParams[i].sync->getProcValue() > 0.0f)
                rate = 1.0f / gin::NoteDuration::getNoteDurations()[size_t(params.randomLfoParams[i].beat->getProcValue())].
                    toSeconds(proc.getPlayHead());
            else
                rate = proc.modMatrix.getValue(params.randomLfoParams[i].rate);
            monoRandomLFOs[i].updateParametersMono(proc.modMatrix, rate);
            monoRandomLFOs[i].process(currentBlockSize);
            proc.modMatrix.setMonoValue(proc.modSrcMonoRND[i], monoRandomLFOs[i].getOutput(0), 0);
            proc.modMatrix.setMonoValue(proc.modSrcMonoRND[i], monoRandomLFOs[i].getOutput(1), 1);
        }
    }

    for(int i = 0; i < NUM_MSEGS; i++)
    {
        if(params.msegParams[i].enabled->isOn())
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

    // for (int i = 0; i < NUM_MSEGS; i++)
    // {
    //     if (params.msegParams[i].enabled->isOn())
    //     {
    //         gin::MSEG::Parameters internalParams;
    //
    //         float freq = 0;
    //         if (params.msegParams[i].sync->getProcValue() > 0.0f)
    //             freq = 1.0f / gin::NoteDuration::getNoteDurations()[size_t(params.msegParams[i].beat->getProcValue())].
    //                 toSeconds(proc.getPlayHead());
    //         else
    //             freq = proc.modMatrix.getValue(params.msegParams[i].rate);
    //
    //         internalParams.frequency = freq;
    //         internalParams.phase = proc.modMatrix.getValue(params.msegParams[i].phase);
    //         internalParams.offset = proc.modMatrix.getValue(params.msegParams[i].offset);
    //         internalParams.depth = proc.modMatrix.getValue(params.msegParams[i].depth);
    //         internalParams.delay = 0;
    //         internalParams.fade = 0;
    //
    //         monoMSEGs.getReference(i).setParameters(internalParams);
    //         monoLFOs[i].process(currentBlockSize);
    //         proc.modMatrix.setMonoValue(proc.modSrcMonoMSEG[i], monoMSEGs.getReference(i).getOutput());
    //     }
    //     else
    //     {
    //         proc.modMatrix.setMonoValue(proc.modSrcMonoMSEG[i], 0);
    //     }
    // }
}

void ResonatorSynth::renderNextSubBlock(juce::AudioBuffer<float>& outputAudio, int startSample, int numSamples)
{
    currentBlockSize = numSamples;
    updateParameters();
    Synthesiser::renderNextSubBlock(outputAudio, startSample, numSamples);
}

void ResonatorSynth::fillExtInExciterBuffers(const juce::AudioBuffer<float>& buffer)
{
    for(auto v : voices)
    {
        // dynamic_cast<ResonatorVoice*>(v)->extInExciter->fillInputBuffer(buffer);
    }
}

void ResonatorSynth::panic()
{
    //kill voices and reset
    for(auto v : voices)
    {
        stopVoiceFastKill(v, v->getCurrentlyPlayingNote(), false);
    }
}
