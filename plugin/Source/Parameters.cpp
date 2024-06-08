//
// Created by Gabriel Soule on 6/4/24.
//

#include "Parameters.h"
#include "PluginProcessor.h"
#include "Resonator.h"

void ExciterParams::setup(ResonariumProcessor& p)
{
    attack = p.addExtParam("exciterAttack", "Attack", "A", "s",
                           {0.0, 30.0, 0.0, 0.2f}, 0.1f,
                           0.0f);

    decay = p.addExtParam("exciterDecay", "Decay", "D", "s",
                          {0.0, 30.0, 0.0, 0.2f}, 0.1f,
                          0.0f);

    sustain = p.addExtParam("exciterSustain", "Sustain", "S", "s",
                            {0.0, 100, 0.0, 1.0}, 50.0f,
                            0.0f);
    sustain->conversionFunction = [](const float x) { return x / 100.0f; };

    release = p.addExtParam("exciterRelease", "Release", "R", "s",
                            {0.0, 30.0, 0.0, 0.2f}, 0.1f,
                            0.0f);

    level = p.addExtParam("exciterLevel", "Level", "L", "dB",
                          {0.0, 60.0, 0.0, 0.2f}, 0.1f,
                          0.0f);
}

ResonatorParams::ResonatorParams(int resonatorIndex, int bankIndex) : resonatorIndex(resonatorIndex),
                                                                      bankIndex(bankIndex)
{
}

void ResonatorParams::setup(ResonariumProcessor& p)
{
    jassert(resonatorIndex >= 0 && resonatorIndex < NUM_RESONATORS);
    jassert(bankIndex >= 0 && bankIndex < NUM_RESONATOR_BANKS);

    juce::String suffix = "_bank" + std::to_string(bankIndex) + "res" + std::to_string(resonatorIndex);

    enabled = p.addExtParam("enabled" + suffix, "Enabled" + suffix, "On/Off", "",
                            {0.0, 1.0, 1.0, 1.0f}, 0.0f,
                            0.0f);

    harmonic = p.addExtParam("harmonic" + suffix, "Pitch Offset" + suffix, "Pitch", " st",
                             {-24.0f, 24.0f, 0.01f, 1.0f}, 0.0f,
                             0.0f);

    decayTime = p.addExtParam("decayTime" + suffix, "Decay Time" + suffix, "Decay", "s",
                              {0.0, 60.0, 0.0, 0.2f}, 3.0f,
                              0.0f);

    dispersion = p.addExtParam("dispersion" + suffix, "Dispersion" + suffix, "Disp.", "%",
                               {0.0, 100.0f, 0.0, 1.0f}, 0.0f,
                               0.0f);
    dispersion->conversionFunction = [](const float x) { return x / 100.0f; };

    decayFilterCutoff = p.addExtParam("decayFilterCutoff" + suffix, "Filter Cutoff" + suffix, "Cutoff", "Hz",
                                      {20.0, 20000.0, 0.0, 1.0f}, 1000.0f,
                                      0.0f);

    // LP, BP, HP, SVF
    decayFilterType = p.addExtParam("decayFilterType" + suffix, "Filter Type" + suffix, "Filter", "",
                                    {0.0, 7.0, 1.0, 1.0f}, 0.0f,
                                    0.0f);

    decayFilterResonance = p.addExtParam("decayFilterResonance" + suffix, "Resonance" + suffix, "Res", "",
                                         {0.0, 100.0f, 0.0, 0.2f}, 0.0f,
                                         0.0f);

    decayFilterKeytrack = p.addExtParam("filter_keytrack" + suffix, "Keytrack" + suffix, "Key Track", "%",
                                        {0.0, 100.0f, 0.0, 1.0f}, 0.0f,
                                        0.0f);

    gain = p.addExtParam("gain" + suffix, "Gain" + suffix, "Gain", "dB",
                         {-100.0, 0.0, 0.0, 4.0f}, 0.0f,
                         0.0f);
    gain->conversionFunction = [](const float x) { return juce::Decibels::decibelsToGain(x); };
}

ResonatorBankParams::ResonatorBankParams(int index) : index(index), resonatorParams{}
{
}


void ResonatorBankParams::setup(ResonariumProcessor& p)
{
    jassert(index >= 0 && index < NUM_RESONATOR_BANKS);
    for (int i = 0; i < NUM_RESONATORS; i++)
    {
        resonatorParams[i] = ResonatorParams(i, index);
        resonatorParams[i].setup(p);
    }

    juce::String suffix = "_bank" + std::to_string(index);

    noteOffset = p.addExtParam("noteOffset" + suffix, "Note Offset" + suffix, "Note", "semitones",
                               {-24.0f, 24.0f, 0.0f, 1.0f}, 0.0f,
                               gin::SmoothingType::linear);

    couplingMode = p.addExtParam("couplingMode" + suffix, "Coupling Mode" + suffix, "Coupling", "",
                                 {0.0, 2.0, 1.0, 1.0f}, 0.0f,
                                 gin::SmoothingType::linear);

    outputGain = p.addExtParam("outputGain" + suffix, "Output Gain" + suffix, "Gain", "dB",
                               {-100.0, 0.0, 0.0, 4.0f}, 0.0f,
                               gin::SmoothingType::linear);
    outputGain->conversionFunction = [](const float x) { return juce::Decibels::decibelsToGain(x); };
}
