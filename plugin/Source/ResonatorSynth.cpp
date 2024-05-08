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
