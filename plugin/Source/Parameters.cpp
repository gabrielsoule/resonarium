//
// Created by Gabriel Soule on 6/4/24.
//

#include "Parameters.h"
#include "PluginProcessor.h"

static juce::String filterTextFunction(const gin::Parameter&, float v)
{
    switch (int(v))
    {
    case 0: return "None";
    case 1: return "LP";
    case 2: return "BP";
    case 3: return "HP";
    case 4: return "Notch";
    case 5: return "Allpass";
    default:
        jassertfalse;
        return {};
    }
}

void MultiFilterParams::setup(ResonariumProcessor& p, juce::String prefix)
{
    this->prefix = prefix;

    type = p.addExtParam(prefix + "filterType", prefix + " Filter Type", "Type", "",
                         {0.0, 5.0f, 1.0, 1.0}, 0.0, 0.0f,
                         filterTextFunction);
    frequency = p.addExtParam(prefix + "frequency", prefix + " Frequency ", "Freq", "Hz",
                              {20.0, 20000.0, 0.0, 1.0f}, 1000.0f,
                              0.0f);
    resonance = p.addExtParam(prefix + "_resonance", prefix + " Resonance", "Res", "",
                              {0.0, 100.0f, 0.0, 0.2f}, 0.0f,
                              0.0f);
}

void ADSRParams::setup(ResonariumProcessor& p, juce::String prefix)
{
    this->prefix = prefix;

    attack = p.addExtParam(prefix + "attack", prefix + " Attack", "A", "s", {0.0, 60.0, 0.0, 0.2f}, 0.1f, 0.0f);
    decay = p.addExtParam(prefix + "decay", prefix + " Decay", "D", "s", {0.0, 60.0, 0.0, 0.2f}, 0.1f, 0.0f);
    sustain = p.addExtParam(prefix + "sustain", prefix + " Sustain", "S", "%", {0.0, 100.0, 0.0, 1.0}, 80.0f, 0.0f);
    release = p.addExtParam(prefix + "release", prefix + " Release", "R", "s", {0.0, 60.0, 0.0, 0.2f}, 0.1f, 0.0f);

    sustain->conversionFunction = [](float in) { return in / 100.0f; };
}

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

void ResonatorParams::setup(ResonariumProcessor& p, int resonatorIndex, int bankIndex)
{
    jassert(resonatorIndex >= 0 && resonatorIndex < NUM_RESONATORS);
    jassert(bankIndex >= 0 && bankIndex < NUM_RESONATOR_BANKS);

    this->resonatorIndex = resonatorIndex;
    this->bankIndex = bankIndex;

    juce::String suffix = " b" + std::to_string(bankIndex) + "r" + std::to_string(resonatorIndex);

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

    testParameter = p.addExtParam("testParameter" + suffix, "Test Parameter" + suffix, "Test", "",
                                  {0.0, 1.0, 0.0, 1.0f}, 0.0f,
                                  0.0f);
}

void ResonatorBankParams::setup(ResonariumProcessor& p, int index)
{
    jassert(index >= 0 && index < NUM_RESONATOR_BANKS);

    this->index = index;

    for (int i = 0; i < NUM_RESONATORS; i++)
    {
        resonatorParams[i].setup(p, i, index);
    }

    juce::String suffix = " b" + std::to_string(index);

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

void ImpulseExciterParams::setup(ResonariumProcessor& p, int index)
{
    this->index = index;
    juce::String prefix = "impExciter" + std::to_string(index) + " ";

    this->thickness = p.addExtParam(prefix + "thickness", prefix + "Thickness", "Density", "",
                                    {1.0f, 10.0, 1.0f, 1.0f}, 1.0f,
                                    0.0f);
    this->pickPosition = p.addExtParam(prefix + "pickPosition", prefix + "Pick Position", "Position", "",
                                       {0.0, 1.0, 0.01, 1.0f}, 0.5f,
                                       0.0f);
    filterParams.setup(p, "impExciter" + std::to_string(index));
}

void NoiseExciterParams::setup(ResonariumProcessor p, int index)
{
    this->index = index;
    juce::String prefix = "noiseExciter" + std::to_string(index) + " ";

    this->type = p.addIntParam(prefix + "type", prefix + "Type", "Type", "",
                               {0.0, 2.0, 1.0, 1.0f}, 0.0f,
                               0.0f);
    this->density = p.addExtParam(prefix + "density", prefix + "Density", "Density", "",
                                  {0.0, 1.0, 0.01, 1.0f}, 0.5f,
                                  0.0f);
    this->gain = p.addExtParam(prefix + "gain", prefix + "Gain", "Gain", "dB",
                               {-100.0, 0.0, 0.0, 4.0f}, 0.0f,
                               0.0f);

    filterParams.setup(p, prefix);
    adsrParams.setup(p,prefix);
}
