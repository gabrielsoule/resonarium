//
// Created by Gabriel Soule on 5/1/24.
//

#include "ResonatorSynth.h"
#include "ResonatorVoice.h"
#include "PluginProcessor.h"

#define NUM_VOICES 16

ResonatorSynth::ResonatorSynth(ResonariumProcessor& p) : processor(p)
{
    //call superclass constructor
    // Synthesiser::Synthesiser();

    DBG("Creating Synthesiser");
}

void ResonatorSynth::prepare(const juce::dsp::ProcessSpec& spec)
{
    setCurrentPlaybackSampleRate(spec.sampleRate);
    for(auto* v : voices)
    {
        dynamic_cast<ResonatorVoice*>(v)->prepare(spec);
    }
}

void ResonatorSynth::setupParameters()
{
    // VoiceParams params;
    // params.setup(processor);
    //
    // for(auto* v : voices)
    // {
    //     ResonatorVoice* voice = dynamic_cast<ResonatorVoice*>(v);
    //     voice->distributeParameters(params);
    // }

    // ImpulseExciterParams impulseExciterParams;
    // impulseExciterParams.setup(processor, 0);
    //
    // NoiseExciterParams noiseExciterParams;
    // noiseExciterParams.setup(processor, 0);
    //
    // for(auto* v : voices)
    // {
    //     ResonatorVoice* voice = dynamic_cast<ResonatorVoice*>(v);
    //     voice->impulseExciter.params = impulseExciterParams;
    //     voice->impulseExciter.filter.params = impulseExciterParams.filterParams;
    //     voice->noiseExciter.params = noiseExciterParams;
    //     voice->noiseExciter.filter.params = noiseExciterParams.filterParams;
    //     jassert(voice != nullptr);
    //     for(int i = 0; i < NUM_RESONATOR_BANKS; i++)
    //     {
    //         voice->resonatorBanks[i]->params = processor.resonatorBanksParams[i];
    //         for(int j = 0; j < NUM_RESONATORS; j++)
    //         {
    //             voice->resonatorBanks[i]->resonators[j]->params = voice->resonatorBanks[i]->params.resonatorParams[j];
    //         }
    //     }
    // }
}
