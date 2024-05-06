//
// Created by Gabriel Soule on 5/1/24.
//

#include "ResonatorSynth.h"
#include "ResonatorVoice.h"
#include "PluginProcessor.h"

#define NUM_VOICES 16

ResonatorSynth::ResonatorSynth(ResonariumProcessor& p) : processor(p)
{

    this->enableLegacyMode();
    this->setVoiceStealingEnabled(true);
    for (int i = 0; i < NUM_VOICES; i++)
    {
        auto voice = new ResonatorVoice(processor);
        this->addVoice(voice);
        processor.modMatrix.addVoice(voice);
    }


}
