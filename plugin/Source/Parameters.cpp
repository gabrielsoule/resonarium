//
// Created by Gabriel Soule on 6/4/24.
//

#include "Parameters.h"
#include "PluginProcessor.h"
#include "dsp/Distortion.h"
#include "dsp/MultiAmp.h"

static juce::String filterTextFunction(const gin::Parameter&, float v)
{
    switch (int(v))
    {
    case 0: return "None";
    case 1: return "LP";
    case 2: return "HP";
    case 3: return "BP";
    case 4: return "NT";
    case 5: return "AP";
    default:
        jassertfalse;
        return {};
    }
}

static juce::String loopFilterTypeTextFunction(const gin::Parameter&, float v)
{
    switch (int(v))
    {
    case 0: return "LP";
    case 1: return "BP";
    case 2: return "HP";
    default:
        jassertfalse;
        return {};
    }
}

static juce::String couplingModeTextFunction(const gin::Parameter&, float v)
{
    switch (int(v))
    {
    case 0: return "Parallel";
    case 1: return "Interlinked";
    case 2: return "Cascade";
    default:
        jassertfalse;
        return {};
    }
}

static juce::String impulseTrainExciterModeTextFunction(const gin::Parameter&, float v)
{
    switch (int(v))
    {
    case 0: return "Impulse";
    case 1: return "Static";
    case 2: return "Pulse";
    case 3: return "Noise Burst";
    default:
        jassertfalse;
        return {};
    }
}

static juce::String randomLFOModeTextFunction(const gin::Parameter&, float v)
{
    switch (int(v))
    {
    case 0: return "Perlin";
    case 1: return "Step";
    case 2: return "S&H";
    default:
        jassertfalse;
        return {};
    }
}

static juce::String enableTextFunction(const gin::Parameter&, float v)
{
    return v > 0.0f ? "On" : "Off";
}

static juce::String durationTextFunction(const gin::Parameter&, float v)
{
    return gin::NoteDuration::getNoteDurations()[size_t(v)].getName();
}

static juce::String lfoTextFunction(const gin::Parameter&, float v)
{
    switch ((gin::LFO::WaveShape)int(v))
    {
    case gin::LFO::WaveShape::none: return "None";
    case gin::LFO::WaveShape::sine: return "Sine";
    case gin::LFO::WaveShape::triangle: return "Triangle";
    case gin::LFO::WaveShape::sawUp: return "Saw Up";
    case gin::LFO::WaveShape::sawDown: return "Saw Down";
    case gin::LFO::WaveShape::square: return "Square";
    case gin::LFO::WaveShape::squarePos: return "Square+";
    case gin::LFO::WaveShape::sampleAndHold: return "S&H";
    case gin::LFO::WaveShape::noise: return "Noise";
    case gin::LFO::WaveShape::stepUp3: return "Step Up 3";
    case gin::LFO::WaveShape::stepUp4: return "Step Up 4";
    case gin::LFO::WaveShape::stepup8: return "Step Up 8";
    case gin::LFO::WaveShape::stepDown3: return "Step Down 3";
    case gin::LFO::WaveShape::stepDown4: return "Step Down 4";
    case gin::LFO::WaveShape::stepDown8: return "Step Down 8";
    case gin::LFO::WaveShape::pyramid3: return "Pyramid 3";
    case gin::LFO::WaveShape::pyramid5: return "Pyramid 5";
    case gin::LFO::WaveShape::pyramid9: return "Pyramid 9";
    default:
        jassertfalse;
        return {};
    }
}

static juce::String multiAmpModeTextFunction(const gin::Parameter&, float v)
{
    switch (MultiAmp::Mode(int(v)))
    {
    case MultiAmp::Mode::BIG: return "Big";
    case MultiAmp::Mode::BASS: return "Bass";
    case MultiAmp::Mode::FIRE: return "Fire";
    case MultiAmp::Mode::LEAD: return "Lead";
    case MultiAmp::Mode::GRIND: return "Grind";
    case MultiAmp::Mode::DEREZ: return "Bitcrush";
    default:
        jassertfalse;
        return {};
    }
}

static juce::String distortionTypeTextFunction(const gin::Parameter&, float v)
{
    switch (Distortion::DistortionMode(int(v)))
    {
    case Distortion::DistortionMode::SOFT_CLIP: return "Soft Clip";
    case Distortion::DistortionMode::HARD_CLIP: return "Hard Clip";
    case Distortion::DistortionMode::LINEAR_FOLD: return "Linear Fold";
    case Distortion::DistortionMode::SIN_FOLD: return "Sine Fold";
    case Distortion::DistortionMode::BIT_CRUSH: return "Bit Crush";
    case Distortion::DistortionMode::DOWN_SAMPLE: return "Down Sample";
    default:
        jassertfalse;
        return {};
    }
}

static juce::String distortionFilterModeTextFunction(const gin::Parameter&, float v)
{
    switch (Distortion::DistortionMode(int(v)))
    {
    case Distortion::FilterMode::disabled: return "Off";
    case Distortion::FilterMode::pre: return "Pre";
    case Distortion::FilterMode::post: return "Post";
    default:
        jassertfalse;
        return {};
    }
}

void MultiFilterParams::setup(ResonariumProcessor& p, juce::String prefix)
{
    this->prefix = prefix;

    type = p.addExtParam(prefix + "filterType", prefix + " Filter Type", "Filter", "",
                         {0.0f, 5.0f, 1.0f, 1.0f},
                         0.0f, 0.0f, "", filterTextFunction);
    frequency = p.addExtParam(prefix + "frequency", prefix + " Frequency ", "Freq", "Hz",
                              {20.0f, 20000.0f, 0.0f, 0.4f},
                              1000.0f, 0.0f);
    resonance = p.addExtParam(prefix + "_resonance", prefix + " Resonance", "Res", "",
                              {0.01f, 100.0f, 0.0f, 0.4f},
                              1.0f / std::sqrt(2.0f), 0.0f);
}

void ADSRParams::setup(ResonariumProcessor& p, juce::String prefix, int index)
{
    this->prefix = prefix;
    this->index = index;

    enabled = p.addExtParam(prefix + "enabled", prefix + " Enabled", "On/Off", "",
                            {0.0f, 1.0f, 1.0f, 1.0f}, 0.0f,
                            0.0f);

    attack = p.addExtParam(prefix + "attack", prefix + " Attack", "A", "s",
                           {0.0f, 60.0f, 0.0f, 0.2f},
                           0.1f, 0.0f);
    decay = p.addExtParam(prefix + "decay", prefix + " Decay", "D", "s",
                          {0.0, 60.0f, 0.0f, 0.2f},
                          0.1f, 0.0f);
    sustain = p.addExtParam(prefix + "sustain", prefix + " Sustain", "S", "%",
                            {0.0f, 100.0f, 0.0f, 1.0f},
                            80.0f, 0.0f);
    release = p.addExtParam(prefix + "release", prefix + " Release", "R", "s",
                            {0.0f, 60.0f, 0.0f, 0.2f},
                            0.1f, 0.0f);

    sustain->conversionFunction = [](float in) { return in / 100.0f; };
}

void ResonatorParams::setup(ResonariumProcessor& p, int resonatorIndex, int bankIndex)
{
    jassert(resonatorIndex >= 0 && resonatorIndex < NUM_RESONATORS);
    jassert(bankIndex >= 0 && bankIndex < NUM_RESONATOR_BANKS);

    this->resonatorIndex = resonatorIndex;
    this->bankIndex = bankIndex;

    juce::String suffix = " wb" + std::to_string(bankIndex) + "r" + std::to_string(resonatorIndex);

    enabled = p.addExtParam("enabled" + suffix, "Enabled" + suffix, "On/Off", " ",
                            {0.0, 1.0, 1.0, 1.0f}, 0.0f,
                            0.0f);

    pitch = p.addExtParam("pitch" + suffix, "Pitch Multiplier" + suffix, "Pitch", "",
                          {0.10, 10.0f, 0.01f, 0.7f}, 1.0f,
                          0.0f, "resonator.pitch");

    frequency = p.addExtParam("resonatorFrequency" + suffix, "Frequency" + suffix, "Freq", "Hz",
                              {20.0f, 20000.0f, 0.0f, 0.4f}, 1000.0f,
                              0.0f, "resonator.frequency");

    resonatorKeytrack = p.addExtParam("resonatorKeytrack" + suffix, "Keytrack" + suffix, "Key Track", "%",
                                      {0.0f, 100.0f, 0.0f, 1.0f}, 1.0f,
                                      0.0f, "resonator.keytrack");

    decayTime = p.addExtParam("decayTime" + suffix, "Decay Time" + suffix, "Decay", "s",
                              {0.0f, 60.0f, 0.0f, 0.2f}, 3.0f,
                              0.0f, "resonator.decay");

    dispersion = p.addExtParam("dispersion" + suffix, "Dispersion" + suffix, "Disp.", "%",
                               {0.0f, 100.0f, 0.0f, 1.0f}, 0.0f,
                               0.0f, "resonator.dispersion");
    dispersion->conversionFunction = [](const float x) { return x / 100.0f; };

    loopFilterCutoff = p.addExtParam("decayFilterCutoff" + suffix, "Loop Filter Cutoff" + suffix, "Cutoff", "Hz",
                                     {20.0f, 20000.0, 0.0f, 0.2f}, 3000.0f,
                                     0.0f, "resonator.loopfiltercutoff");

    loopFilterPitchInSemis = p.addExtParam("decayFilterPitch" + suffix, "Loop Filter Pitch" + suffix, "Pitch", "ST",
                                           {-60.0f, 60.0f, 0.01f, 1.0f}, 0.0f,
                                           0.0f, "resonator.loopfiltercutoff");

    loopFilterResonance = p.addExtParam("decayFilterResonance" + suffix, "Loop Filter Resonance" + suffix, "Res", "",
                                        {0.0f, 100, 0.0f, 0.2f}, 0,
                                        0.0f, "resonator.loopfilterresonance");
    constexpr float one_over_sqrt2 = 0.7071067811865475244f;
    loopFilterResonance->conversionFunction = [](const float x) { return x + one_over_sqrt2; };

    loopFilterKeytrack = p.addIntParam("decayFilterKeytrack" + suffix, "Loop Filter Keytrack" + suffix, "Key Track", "",
                                       {0.0f, 1.0f, 0.0f, 1.0f}, 0.0f,
                                       0.0f);

    loopFilterMode = p.addExtParam("svfMode" + suffix, "Mode" + suffix, "Loop Filter Mode", "",
                                   {0.0f, 1.0f, 0.01, 1.0f}, 0.0f,
                                   0.0f);
    loopFilterType = p.addExtParam("svfType" + suffix, "Type" + suffix, "Loop Filter Type", "",
                                   {0.0f, 2.0f, 1.0, 1.0f}, 0.0f,
                                   0.0f, "", loopFilterTypeTextFunction);

    postFilterCutoff = p.addExtParam("postFilterCutoff" + suffix, "Post Filter Cutoff" + suffix, "Cutoff", "Hz",
                                     {20.0f, 20000.0f, 0.0f, 0.2f}, 9000.0f,
                                     0.0f);

    postFilterPitchInSemis = p.addExtParam("postFilterPitch" + suffix, "Post Filter Pitch" + suffix, "Pitch", "ST",
                                           {-60.0f, 60.0f, 0.01f, 1.0f}, 0.0f,
                                           0.0f);

    postFilterResonance = p.addExtParam("postFilterResonance" + suffix, "Post Filter Resonance" + suffix, "Res", "",
                                        {0.01f, 800.0f, 0.0f, 0.2f}, 1.0f / std::sqrt(2.0f),
                                        0.0f);

    postFilterMode = p.addExtParam("postFilterMode" + suffix, "Post Filter Mode" + suffix, "Mode", "",
                                   {0.0f, 1.0f, 0.0, 1.0f}, 0.0f,
                                   0.0f);

    postFilterKeytrack = p.addIntParam("postFilterKeytrack" + suffix, "Post Filter Keytrack" + suffix, "Key Track", "",
                                       {0.0f, 1.0f, 0.0f, 1.0f}, 0.0f,
                                       0.0f);

    gain = p.addExtParam("gain" + suffix, "Gain" + suffix, "Gain", "dB",
                         {-100.0f, 0.0f, 0.0f, 4.0f}, 0.0f,
                         0.0f, "resonator.gain");
    gain->conversionFunction = [](const float x) { return juce::Decibels::decibelsToGain(x); };
}

void WaveguideResonatorBankParams::setup(ResonariumProcessor& p, int index)
{
    jassert(index >= 0 && index < NUM_RESONATOR_BANKS);
    this->index = index;
    for (int i = 0; i < NUM_RESONATORS; i++)
    {
        resonatorParams[i].setup(p, i, index);
    }

    juce::String suffix = " wb" + std::to_string(index);

    noteOffset = p.addExtParam("noteOffset" + suffix, "Note Offset" + suffix, "Note", "semitones",
                               {-36.0f, 36.0f, 0.0f, 1.0f}, 0.0f,
                               gin::SmoothingType::linear);

    useSemitones = p.addIntParam("useSemitones" + suffix, "Use Semitones" + suffix, "Use Semitones", "",
                                 {0.0f, 1.0f, 0.0f, 1.0f}, 1.0f,
                                 0.0f, "resonatorbank.usesemitones");

    couplingMode = p.addExtParam("couplingMode" + suffix, "Coupling Mode" + suffix, "Coupling", "",
                                 {0.0, 2.0, 1.0, 1.0f}, 0.0f,
                                 gin::SmoothingType::linear, "resonatorbank.coupling", couplingModeTextFunction);

    inputGain = p.addExtParam("inputGain" + suffix, "Input Gain" + suffix, "Gain In", "dB",
                              {-100.0, 100.0, 0.0, 1.0f}, 0.0f,
                              gin::SmoothingType::linear, "resonatorbank.inputgain");
    inputGain->conversionFunction = [](const float x) { return juce::Decibels::decibelsToGain(x); };

    //input mix between the exciter and the previous resonator bank
    inputMix = p.addExtParam("inputMix" + suffix, "Input Mix" + suffix, "Input Mix", "",
                             {0.0, 1.0, 0.01, 1.0f}, 0.0f,
                             gin::SmoothingType::linear, "resonatorbank.inputmix");

    outputGain = p.addExtParam("outputGain" + suffix, "Output Gain" + suffix, "Gain Out", "dB",
                               {-100.0, 100.0, 0.0, 1.0f}, 0.0f,
                               gin::SmoothingType::linear, "resonatorbank.outputgain");
    outputGain->conversionFunction = [](const float x) { return juce::Decibels::decibelsToGain(x); };

    cascadeLevel = p.addExtParam("cascadeAmount" + suffix, "Cascade Amount" + suffix, "Amount", "",
                                 {-1.0, 1.0, 0.01, 1.0f}, 0.0f,
                                 gin::SmoothingType::linear, "resonatorbank.cascadeamount");

    cascadeFilterCutoff = p.addExtParam("cascadeFilterCutoff" + suffix, "Cascade Filter Cutoff" + suffix, "Cutoff",
                                        "Hz",
                                        {20.0f, 20000.0f, 0.0f, 0.4f}, 3000.0f,
                                        gin::SmoothingType::linear, "resonatorbank.cascadefiltercutoff");

    cascadeFilterResonance = p.addExtParam("cascadeFilterResonance" + suffix, "Cascade Filter Resonance" + suffix,
                                           "Res", "",
                                           {0.01f, 800.0f, 0.0f, 0.4f}, 1.0f / std::sqrt(2.0f),
                                           gin::SmoothingType::linear, "resonatorbank.cascadefilterresonance");

    cascadeFilterMode = p.addExtParam("cascadeFilterMode" + suffix, "Cascade Filter Mode" + suffix, "Mode", "",
                                      {0.0f, 1.0f, 0.0, 1.0f}, 0.0f,
                                      0.0f, "resonatorbank.cascadefiltermode");
}

void ImpulseExciterParams::setup(ResonariumProcessor& p, int index)
{
    this->index = index;
    juce::String prefix = "impExciter" + std::to_string(index) + " ";

    this->enabled = p.addExtParam(prefix + "enabled", prefix + " Enabled", "On/Off", "",
                                  {0.0f, 1.0f, 1.0f, 1.0f}, 0.0f,
                                  0.0f);

    this->thickness = p.addExtParam(prefix + "thickness", prefix + "Thickness", "Density", "",
                                    {1.0f, 10.0f, 1.0f, 1.0f}, 1.0f,
                                    0.0f);
    this->pickPosition = p.addExtParam(prefix + "pickPosition", prefix + "Pick Position", "Position", "",
                                       {0.0f, 1.0f, 0.01f, 1.0f}, 0.5f,
                                       0.0f);
    this->level = p.addExtParam(prefix + "level", prefix + " Level", "Level", "",
                                {0.0f, 1.0f, 0.01f, 1.0f}, 1.0f,
                                0.0f);
    // level->conversionFunction = [](const float x) { return juce::Decibels::decibelsToGain(x); };

    filterParams.setup(p, "impExciter" + std::to_string(index));
}

void NoiseExciterParams::setup(ResonariumProcessor& p, int index)
{
    this->index = index;
    juce::String prefix = "noiseExciter" + std::to_string(index);

    this->enabled = p.addExtParam(prefix + "enabled", prefix + " Enabled", "On/Off", "",
                                  {0.0f, 1.0f, 1.0f, 1.0f}, 0.0f,
                                  0.0f);

    this->type = p.addIntParam(prefix + "type", prefix + " Type", "Type", "",
                               {0.0f, 2.0f, 1.0f, 1.0f}, 0.0f,
                               0.0f);
    this->density = p.addExtParam(prefix + "density", prefix + " Density", "Density", "",
                                  {0.0f, 1.0f, 0.01f, 1.0f}, 0.5f,
                                  0.0f);
    this->level = p.addExtParam(prefix + "level", prefix + " Level", "Level", "",
                                {0.0f, 1.0f, 0.01f, 1.0f}, 1.0f,
                                0.0f);
    // level->conversionFunction = [](const float x) { return juce::Decibels::decibelsToGain(x); };

    adsrParams.setup(p, "noiseExciter1Env" + juce::String(index));
    filterParams.setup(p, prefix);
}

void ImpulseTrainExciterParams::setup(ResonariumProcessor& p, int index)
{
    this->index = index;
    juce::String prefix = "impTrainExciter" + juce::String(index);

    this->enabled = p.addExtParam(prefix + "enabled", prefix + " Enabled", "On/Off", "",
                                  {0.0f, 1.0f, 1.0f, 1.0f}, 0.0f,
                                  0.0f);

    this->mode = p.addExtParam(prefix + "mode", prefix + " Mode", "Mode", "",
                               {0.0f, 3.0f, 1.0f, 1.0f}, 0.0f,
                               0.0f, "", impulseTrainExciterModeTextFunction);

    this->rate = p.addExtParam(prefix + "rate", prefix + " Rate", "Rate", "Hz",
                               {0.1f, 1000.0f, 0.01f, 0.4f}, 1.0f,
                               0.0f);

    this->sync = p.addExtParam(prefix + "sync", prefix + " Sync", "Sync", "",
                               {0.0f, 1.0f, 0.01f, 1.0f}, 0.0f,
                               0.0f);

    this->entropy = p.addExtParam(prefix + "entropy", prefix + " Entropy", "Rnd", "",
                                  {0.0f, 1.0f, 0.01f, 1.0f}, 0.0f,
                                  0.0f);

    this->character = p.addExtParam(prefix + "character", prefix + " Character", "Mod", "",
                                    {0.0f, 1.0f, 0.01f, 1.0f}, 0.0f,
                                    0.0f);

    this->level = p.addExtParam(prefix + "level", prefix + " Level", "Level", "",
                                {0.0f, 1.0f, 0.01f, 1.0f}, 1.0f,
                                0.0f);
    // level->conversionFunction = [](const float x) { return juce::Decibels::decibelsToGain(x); };

    adsrParams.setup(p, "impTrainExciterEnv" + juce::String(index));
    filterParams.setup(p, prefix);
}

void ExternalInputExciterParams::setup(ResonariumProcessor& p)
{
    filterParams.setup(p, "Ext. In");

    enabled = p.addExtParam("extInEnable", "Ext. In Enable", "Enable", "",
                            {0.0f, 1.0f, 1.0f, 1.0f}, 0.0f,
                            0.0f, "", enableTextFunction);

    gain = p.addExtParam("extInGain", "Ext. In Gain", "Gain", "dB",
                         {-100.0f, 24.0f, 0.0f, 4.0f}, 0.0f,
                         0.0f);
    gain->conversionFunction = [](const float x) { return juce::Decibels::decibelsToGain(x); };

    mix = p.addExtParam("extInMix", "Ext. In Mix", "Mix", "",
                        {0.0f, 1.0f, 0.01f, 1.0f}, 1.0f,
                        0.0f);
}

void SampleExciterParams::setup(ResonariumProcessor& p)
{
    filterParams.setup(p, "Sample");
    adsrParams.setup(p, "Sample");
    enabled = p.addExtParam("sampleEnable", "Sample Enable", "Enable", "",
                            {0.0f, 1.0f, 1.0f, 1.0f}, 0.0f,
                            0.0f, "", enableTextFunction);

    level = p.addExtParam("sampleGain", "Sample Level", "Level", "",
                         {0.0f, 1.0f, 0.0f, 1.0f}, 1.0f,
                         0.0f, "exciter.sampler.gain");

    mix = p.addExtParam("sampleMix", "Sample Mix", "Mix", "",
                        {0.0f, 1.0f, 0.01f, 1.0f}, 1.0f,
                        0.0f);

    loop = p.addIntParam("sampleLoop", "Sample Loop", "Loop", "",
                         {0.0f, 1.0f, 1.0f, 1.0f}, 0.0f,
                         0.0f, "exciter.sampler.loop", enableTextFunction);

    start = p.addExtParam("sampleStart", "Sample Start", "Start", "%",
                          {0.0f, 1.0f, 0.0f, 1.0f}, 0.0f,
                          0.0f, "exciter.sampler.start");
    end = p.addExtParam("sampleEnd", "Sample End", "End", "%",
                        {0.0f, 1.0f, 0.0f, 1.0f}, 1.0f,
                        0.0f, "exciter.sampler.end");
}

void LFOParams::setup(ResonariumProcessor& p, int index)
{
    this->index = index;
    juce::String prefix = "lfo" + std::to_string(index);
    auto notes = gin::NoteDuration::getNoteDurations();

    enabled = p.addExtParam(prefix + "enable", prefix + "Enable", "Enable", "",
                            {0.0f, 1.0f, 1.0f, 1.0f}, 0.0f,
                            0.0f, "", enableTextFunction);

    sync = p.addIntParam(prefix + "sync", prefix + "Sync", "Sync", "",
                         {0.0f, 1.0f, 1.0f, 1.0f}, 0.0f,
                         0.0f, "", enableTextFunction);

    retrig = p.addIntParam(prefix + "retrig", prefix + "Retrig", "Retrig", "",
                           {0.0f, 1.0f, 1.0f, 1.0f}, 1.0f,
                           0.0f, "", enableTextFunction);

    wave = p.addIntParam(prefix + "wave", prefix + "Wave", "Wave", "",
                         {1.0f, 17.0f, 1.0f, 1.0f}, 1.0f,
                         0.0f, "", lfoTextFunction);

    rate = p.addExtParam(prefix + "rate", prefix + "Rate", "Rate", "Hz",
                         {0.0f, 200.0f, 0.0f, 0.3f},
                         1.0f, 0.0f);

    beat = p.addIntParam(prefix + "beat", prefix + "Beat", "Beat", "",
                         {0.0f, float(notes.size() - 1), 1.0f, 1.0f},
                         13.0f, 0.0f, "", durationTextFunction);

    depth = p.addExtParam(prefix + "depth", prefix + "Depth", "Depth", "",
                          {-1.0f, 1.0f, 0.0f, 1.0f},
                          1.0f, 0.0f);

    phase = p.addExtParam(prefix + "phase", prefix + "Phase", "Phase", "",
                          {-1.0f, 1.0f, 0.0f, 1.0f},
                          0.0f, 0.0f);

    offset = p.addExtParam(prefix + "offset", prefix + "Offset", "Offset", "",
                           {-1.0f, 1.0f, 0.0f, 1.0f},
                           0.0f, 0.0f);

    fade = p.addExtParam(prefix + "fade", prefix + "Fade", "Fade", "s",
                         {-60.0f, 60.0f, 0.0f, 0.2f, true},
                         0.0f, 0.0f);

    delay = p.addExtParam(prefix + "delay", prefix + "Delay", "Delay", "s",
                          {0.0f, 60.0f, 0.0f, 0.2f},
                          0.0f, 0.0f);

    stereo = p.addExtParam(prefix + "stereo", prefix + "Stereo", "Stereo", "",
                           {0.0f, 1.0f, 0.01f, 1.0f},
                           0.0f, 0.0f);
    stereo->conversionFunction = [](const float x) { return x * 0.5f; };
}

void RandomLFOParams::setup(ResonariumProcessor& p, int index)
{
    this->index = index;
    seed = juce::Random::getSystemRandom().nextInt();
    juce::String prefix = "rnd" + std::to_string(index);
    auto notes = gin::NoteDuration::getNoteDurations();

    enabled = p.addExtParam(prefix + "enable", prefix + "Enable", "Enable", "",
                            {0.0f, 1.0f, 1.0f, 1.0f}, 0.0f,
                            0.0f, "", enableTextFunction);

    mode = p.addIntParam(prefix + "mode", prefix + "Mode", "Mode", "",
                         {0.0f, 2.0f, 1.0f, 1.0f}, 0.0f,
                         0.0f, "", randomLFOModeTextFunction);

    sync = p.addIntParam(prefix + "sync", prefix + "Sync", "Sync", "",
                         {0.0f, 1.0f, 1.0f, 1.0f}, 0.0f,
                         0.0f, "", enableTextFunction);

    rate = p.addExtParam(prefix + "rate", prefix + "Rate", "Rate", "Hz",
                         {0.0f, 200.0f, 0.0f, 0.3f},
                         1.0f, 0.0f);

    beat = p.addIntParam(prefix + "beat", prefix + "Beat", "Beat", "",
                         {0.0f, float(notes.size() - 1), 1.0f, 1.0f},
                         13.0f, 0.0f, "", durationTextFunction);

    depth = p.addExtParam(prefix + "depth", prefix + "Depth", "Depth", "",
                          {-1.0f, 1.0f, 0.0f, 1.0f},
                          1.0f, 0.0f);

    offset = p.addExtParam(prefix + "offset", prefix + "Offset", "Offset", "",
                           {-1.0f, 1.0f, 0.0f, 1.0f},
                           0.0f, 0.0f);

    smooth = p.addExtParam(prefix + "smooth", prefix + "Smooth", "Smooth", "%",
                           {0.0f, 100.0f, 1.0f, 1.0f},
                           100.0f, 0.0f);
    smooth->conversionFunction = [](const float x) { return x / 100.0f; };

    jitter = p.addExtParam(prefix + "jitter", prefix + "Jitter", "Jitter", "%",
                           {0.0f, 1.0f, 0.01f, 1.0f},
                           0.0f, 0.0f);

    chaos = p.addExtParam(prefix + "chaos", prefix + "chaos", "Chaos", "%",
                          {0.0f, 1.0f, 0.01f, 1.0f},
                          1.0f, 0.0f);

    stereo = p.addExtParam(prefix + "stereo", prefix + "Stereo", "Stereo", "",
                           {0.0f, 1.0f, 0.01f, 1.0f},
                           0.0f, 0.0f);
}

void MSEGParams::setup(ResonariumProcessor& p, int index)
{
    this->index = index;
    juce::String prefix = "mseg" + std::to_string(index);
    msegData = std::make_shared<gin::MSEG::Data>();
    msegData->reset();
    //TODO load the msegData from disk once we figure out how that works...
    enabled = p.addExtParam(prefix + "enable", "MSEG" + std::to_string(index) + "Enable", "Enable", "",
                            {0.0f, 1.0f, 1.0f, 1.0f}, 0.0f,
                            0.0f, "", enableTextFunction);

    sync = p.addIntParam(prefix + "sync", "MSEG" + std::to_string(index) + "Sync", "Sync", "",
                         {0.0f, 1.0f, 1.0f, 1.0f}, 0.0f,
                         0.0f, "", enableTextFunction);

    rate = p.addExtParam(prefix + "rate", "MSEG" + std::to_string(index) + "Rate", "Rate", "Hz",
                         {0.0f, 200.0f, 0.0f, 0.3f},
                         10.0f, 0.0f);

    beat = p.addIntParam(prefix + "beat", "MSEG" + std::to_string(index) + "Beat", "Beat", "",
                         {0.0f, 13.0f, 1.0f, 1.0f},
                         13.0f, 0.0f, "", durationTextFunction);

    depth = p.addExtParam(prefix + "depth", "MSEG" + std::to_string(index) + "Depth", "Depth", "",
                          {-1.0f, 1.0f, 0.0f, 1.0f},
                          1.0f, 0.0f);

    offset = p.addExtParam(prefix + "offset", "MSEG" + std::to_string(index) + "Offset", "Offset", "",
                           {-1.0f, 1.0f, 0.0f, 1.0f},
                           0.0f, 0.0f);

    fade = p.addExtParam(prefix + "fade", "MSEG" + std::to_string(index) + "Fade", "Fade", "s",
                         {-60.0f, 60.0f, 0.0f, 0.2f, true},
                         0.0f, 0.0f);

    phase = p.addExtParam(prefix + "phase", "MSEG" + std::to_string(index) + "Phase", "Phase", "",
                          {-1.0f, 1.0f, 0.0f, 1.0f},
                          0.0f, 0.0f);

    xgrid = p.addExtParam(prefix + "xgrid", "MSEG" + std::to_string(index) + "XGrid", "XGrid", "",
                          {1.0f, 32.0f, 1.0f, 1.0f},
                          8.0f, 0.0f);

    ygrid = p.addExtParam(prefix + "ygrid", "MSEG" + std::to_string(index) + "YGrid", "YGrid", "",
                          {1.0f, 32.0f, 1.0f, 1.0f},
                          1.0f, 0.0f);

    loop = p.addIntParam(prefix + "loop", "MSEG" + std::to_string(index) + "Loop", "Loop", "",
                         {0.0f, 1.0f, 1.0f, 1.0f}, 0.0f,
                         0.0f, "", enableTextFunction);
}

void VoiceParams::setup(ResonariumProcessor& p)
{
    for (int i = 0; i < NUM_RESONATOR_BANKS; i++)
    {
        waveguideResonatorBankParams[i].setup(p, i);
    }
    for (int i = 0; i < NUM_IMPULSE_EXCITERS; i++)
    {
        impulseExciterParams[i].setup(p, i);
    }
    for (int i = 0; i < NUM_NOISE_EXCITERS; i++)
    {
        noiseExciterParams[i].setup(p, i);
    }
    for (int i = 0; i < NUM_IMPULSE_TRAIN_EXCITERS; i++)
    {
        impulseTrainExciterParams[i].setup(p, i);
    }

    externalInputExciterParams.setup(p);
    sampleExciterParams.setup(p);
    //Do NOT set up LFO params; these are set later by the enclosing synth
}

void ChorusParams::setup(ResonariumProcessor& p)
{
    enabled = p.addExtParam("chorusEnable", "Chorus Enable", "Enable", "",
                            {0.0f, 1.0f, 1.0f, 1.0f}, 0.0f,
                            0.0f, "", enableTextFunction);


    sync = p.addIntParam("chorusSync", "Chorus Sync", "Sync", "",
                         {0.0f, 1.0f, 1.0f, 1.0f}, 0.0f,
                         0.0f, "", enableTextFunction);

    beat = p.addExtParam("chorusBeat", "Chorus Beat", "Beat", "",
                         {0.0f, 13.0f, 1.0f, 1.0f},
                         13.0f, 0.0f, "", durationTextFunction);

    rate = p.addExtParam("chorusRate", "Chorus Rate", "Rate", "Hz",
                         {0.0f, 100.0f, 0.0f, 0.3f},
                         0.5f, 0.0f);

    depth = p.addExtParam("chorusDepth", "Chorus Depth", "Depth", "",
                          {0.0f, 1.0f, 0.0f, 1.0f},
                          0.5f, 0.0f);

    delay = p.addExtParam("chorusDelay", "Chorus Delay", "Delay", "s",
                          {0.0f, 0.1f, 0.0f, 1.0f},
                          0.0f, 0.0f);

    feedback = p.addExtParam("chorusFeedback", "Chorus Feedback", "Feedback", "",
                             {-1.0f, 1.0f, 0.0f, 1.0f},
                             0.0f, 0.0f);

    mix = p.addExtParam("chorusMix", "Chorus Mix", "Mix", "",
                        {0.0f, 1.0f, 0.01f, 1.0f},
                        0.5f, 0.0f);
}

void DelayParams::setup(ResonariumProcessor& p)
{
    enabled = p.addIntParam("delayEnable", "Delay Enable", "Enable", "",
                            {0.0f, 1.0f, 1.0f, 1.0f}, 0.0f,
                            0.0f, "", enableTextFunction);

    timeL = p.addExtParam("delayTimeL", "Delay Time L", "Time L", "s",
                          {0.01f, 4.0f, 0.0f, 0.4f},
                          0.5f, 0.0f);

    timeR = p.addExtParam("delayTimeR", "Delay Time R", "Time R", "s",
                          {0.01f, 4.0f, 0.0f, 0.4f},
                          0.5f, 0.0f);

    beatL = p.addExtParam("delayBeatL", "Delay Beat L", "Beat L", "",
                          {0.0f, 13.0f, 1.0f, 1.0f},
                          13.0f, 0.0f, "", durationTextFunction);

    beatR = p.addExtParam("delayBeatR", "Delay Beat R", "Beat R", "",
                          {0.0f, 13.0f, 1.0f, 1.0f},
                          13.0f, 0.0f, "", durationTextFunction);

    pingPongAmount = p.addExtParam("delayPingPong", "Delay PingPong", "PingPong", "",
                                   {0.0f, 1.0f, 0.01f, 1.0f},
                                   0.0f, 0.0f);

    syncL = p.addIntParam("delaySyncL", "Delay Sync L", "Sync L", "",
                          {0.0f, 1.0f, 1.0f, 1.0f}, 0.0f,
                          0.0f, "", enableTextFunction);

    syncR = p.addIntParam("delaySyncR", "Delay Sync R", "Sync R", "",
                          {0.0f, 1.0f, 1.0f, 1.0f}, 0.0f,
                          0.0f, "", enableTextFunction);

    feedback = p.addExtParam("delayFeedback", "Delay Feedback", "Feedback", "",
                             {0.0f, 1.0f, 0.0f, 1.0f},
                             0.5f, 0.0f);

    lock = p.addIntParam("delayLock", "Delay Stereo Lock", "Lock", "",
                         {0.0f, 1.0f, 1.0f, 1.0f}, 1.0f,
                         0.0f, "", enableTextFunction);

    mix = p.addExtParam("delayMix", "Delay Mix", "Mix", "",
                        {0.0f, 1.0f, 0.01f, 1.0f},
                        0.5f, 0.0f);
}

void DistortionParams::setup(ResonariumProcessor& p)
{
    enabled = p.addIntParam("distEnable", "Distortion Enable", "Enable", "",
                            {0.0f, 1.0f, 1.0f, 1.0f}, 0.0f,
                            0.0f, "", enableTextFunction);

    distortionMode = p.addIntParam("distType", "Distortion Type", "Type", "",
                                   {0.0f, 5.0f, 1.0f, 1.0f}, 0.0f,
                                   0.0f, "", distortionTypeTextFunction);

    drive = p.addExtParam("distDrive", "Distortion Drive", "Drive", "dB",
                          {-36.0f, 36.0f, 0.0f, 1.0f}, 0.0f,
                          0.0f);

    outputGain = p.addExtParam("distOutputGain", "Distortion Output Gain", "Output", "dB",
                               {-36.0f, 36.0f, 0.0f, 1.0f}, 0.0f,
                               0.0f);

    mix = p.addExtParam("distMix", "Distortion Mix", "Mix", "",
                        {0.0f, 1.0f, 0.01f, 1.0f},
                        1.0f, 0.0f);

    prePostFilter = p.addIntParam("distPrePostFilter", "Distortion Pre/Post Filter", "Pre/Post", "",
                                  {0.0f, 2.0f, 1.0f, 1.0f}, 0.0f,
                                  0.0f, "", distortionFilterModeTextFunction);

    cutoff = p.addExtParam("distCutoff", "Distortion Cutoff", "Cutoff", "Hz",
                           {20.0f, 20000.0f, 0.0f, 0.4f}, 3000.0f,
                           0.0f);

    resonance = p.addExtParam("distResonance", "Distortion Resonance", "Res.", "",
                              {0.01f, 15.0f, 0.0f, 0.4f}, 1.0f / std::sqrt(2.0f),
                              0.0f);

    filterMode = p.addExtParam("distFilterMode", "Distortion Filter Mode", "Mode", "",
                               {0.0f, 1.0f, 0.0, 1.0f}, 0.0f,
                               0.0f);
}

void MultiAmpParams::setup(ResonariumProcessor& p)
{
    enabled = p.addIntParam("ampEnable", "Amp Enable", "Enable", "",
                            {0.0f, 1.0f, 1.0f, 1.0f}, 0.0f,
                            0.0f, "", enableTextFunction);

    mode = p.addIntParam("ampMode", "Amp Mode", "Mode", "",
                         {0.0f, 5.0f, 1.0f, 1.0f}, 0.0f,
                         0.0f, "", multiAmpModeTextFunction);

    paramA = p.addExtParam("ampParamA", "Amp Param A", "Param A", "",
                           {0.0f, 1.0f, 0.0f, 1.0f}, 0.5f,
                           0.0f);

    paramB = p.addExtParam("ampParamB", "Amp Param B", "Param B", "",
                           {0.0f, 1.0f, 0.0f, 1.0f}, 0.5f,
                           0.0f);

    paramC = p.addExtParam("ampParamC", "Amp Param C", "Param C", "",
                           {0.0f, 1.0f, 0.0f, 1.0f}, 0.5f,
                           0.0f);

    paramD = p.addExtParam("ampParamD", "Amp Param D", "Param D", "",
                           {0.0f, 1.0f, 0.0f, 1.0f}, 0.5f,
                           0.0f);
}

void PhaserParams::setup(ResonariumProcessor& p)
{
    enabled = p.addIntParam("phaserEnable", "Phaser Enable", "Enable", "",
                            {0.0f, 1.0f, 1.0f, 1.0f}, 0.0f,
                            0.0f, "", enableTextFunction);

    sync = p.addIntParam("phaserSync", "Phaser Sync", "Sync", "",
                         {0.0f, 1.0f, 1.0f, 1.0f}, 0.0f,
                         0.0f, "", enableTextFunction);

    beat = p.addExtParam("phaserBeat", "Phaser Beat", "Beat", "",
                         {0.0f, 13.0f, 1.0f, 1.0f},
                         13.0f, 0.0f, "", durationTextFunction);

    rate = p.addExtParam("phaserRate", "Phaser Rate", "Rate", "Hz",
                         {0.0f, 99.99f, 0.0f, 0.3f},
                         0.5f, 0.0f);

    depth = p.addExtParam("phaserDepth", "Phaser Depth", "Depth", "",
                          {0.0f, 1.0f, 0.0f, 1.0f},
                          0.5f, 0.0f);

    centreFrequency = p.addExtParam("phaserCentreFreq", "Phaser Centre Freq", "Centre Freq", "Hz",
                                    {20.0f, 20000.0f, 0.0f, 0.4f}, 3000.0f,
                                    0.0f);

    feedback = p.addExtParam("phaserFeedback", "Phaser Feedback", "Feedback", "",
                             {-1.0f, 1.0f, 0.0f, 1.0f},
                             0.0f, 0.0f);

    mix = p.addExtParam("phaserMix", "Phaser Mix", "Mix", "",
                        {0.0f, 1.0f, 0.01f, 1.0f},
                        0.5f, 0.0f);
}

void CompressorParams::setup(ResonariumProcessor& p)
{
    enabled = p.addIntParam("compressorEnable", "Compressor Enable", "Enable", "",
                            {0.0f, 1.0f, 1.0f, 1.0f}, 0.0f,
                            0.0f, "", enableTextFunction);

    threshold = p.addExtParam("compressorThreshold", "Compressor Threshold", "Thresh", "dB",
                              {-60.0f, 12.0f, 0.0f, 1.0f}, 0.0f,
                              0.0f);

    ratio = p.addExtParam("compressorRatio", "Compressor Ratio", "Ratio", "",
                          {1.0f, 9.0f, 0.0f, 1.0f}, 1.0f,
                          0.0f);

    attack = p.addExtParam("compressorAttack", "Compressor Attack", "Attack", "ms",
                           {1.0f, 1000.0f, 0.0f, 1.0f}, 1.0f,
                           0.0f);

    release = p.addExtParam("compressorRelease", "Compressor Release", "Release", "ms",
                            {1.0f, 1000.0f, 0.0f, 1.0f}, 1.0f,
                            0.0f);
}

void ReverbParams::setup(ResonariumProcessor& p)
{
    enabled = p.addIntParam("reverbEnable", "Reverb Enable", "Enable", "",
                            {0.0f, 1.0f, 1.0f, 1.0f}, 0.0f,
                            0.0f, "", enableTextFunction);

    dampingFreq = p.addExtParam("reverbDampingFreq", "Reverb Damping Freq", "Damping", "Hz",
                                {0.0f, 1.0f, 0.0f, 1.0f}, 0.2f,
                                0.0f);

    density = p.addExtParam("reverbDensity", "Reverb Density", "Density", "",
                            {0.0f, 1.0f, 0.0f, 1.0f}, 0.5f,
                            0.0f);

    bandwidthFreq = p.addExtParam("reverbBandwidthFreq", "Reverb Bandwidth Freq", "BW", "Hz",
                                  {0.0f, 1.0f, 0.0f, 1.0f}, 0.2f,
                                  0.0f);

    decay = p.addExtParam("reverbDecay", "Reverb Decay", "Decay", "",
                          {0.0f, 1.0f, 0.0f, 1.0f}, 0.5f,
                          0.0f);

    predelay = p.addExtParam("reverbPredelay", "Reverb Predelay", "Predelay", "s",
                             {0.0f, 1.0f, 0.0f, 1.0f}, 0.0f,
                             0.0f);

    size = p.addExtParam("reverbSize", "Reverb Size", "Size", "",
                         {0.0f, 1.0f, 0.0f, 1.0f}, 0.5f,
                         0.0f);

    earlyMix = p.addExtParam("reverbEarlyMix", "Reverb Early Mix", "Early Mix", "",
                             {0.0f, 1.0f, 0.0f, 1.0f}, 0.5f,
                             0.0f);

    mix = p.addExtParam("reverbMix", "Reverb Mix", "Mix", "",
                        {0.0f, 1.0f, 0.01f, 1.0f},
                        0.5f, 0.0f);
}

void SVFParams::setup(ResonariumProcessor& p, juce::String name)
{
    juce::String prefix = name + " ";

    enabled = p.addIntParam(prefix + "enable", prefix + "Enable", "Enable", "",
                            {0.0f, 1.0f, 1.0f, 1.0f}, 0.0f,
                            0.0f, "", enableTextFunction);

    cutoff = p.addExtParam(prefix + "cutoff", prefix + "Freq", "Freq", "Hz",
                           {20.0f, 20000.0f, 0.0f, 0.4f}, 20000.0f,
                           0.0f);

    mode = p.addExtParam(prefix + "mode", prefix + "Mode", "Mode", "",
                         {0.0f, 1.0f, 0.0f, 1.0f}, 0.0f,
                         0.0f);

    resonance = p.addExtParam(prefix + "resonance", prefix + "Resonance", "Res", "",
                              {0.0f, 100.0f, 0.0f, 1.0f}, 1.0f / std::sqrt(2.0f),
                              0.0f);
}

void EffectChainParams::setup(ResonariumProcessor& p)
{
    chorusParams.setup(p);
    phaserParams.setup(p);
    reverbParams.setup(p);
    delayParams.setup(p);
    distortionParams.setup(p);
    multiAmpParams.setup(p);
    compressorParams.setup(p);
    filterParams[0].setup(p, "Filter 1");
    filterParams[1].setup(p, "Filter 2");
}

void SynthParams::setup(ResonariumProcessor& p)
{
    voiceParams.setup(p);
    effectChainParams.setup(p);
    voiceParams.effectChainParams = effectChainParams;
    for (int i = 0; i < NUM_LFOS; i++)
    {
        lfoParams[i].setup(p, i);
        //ensure that the same parameters are used for the synth-level monophonic LFOs
        //and the voice-level polyphonic LFOs
        //the result is that each LFO is actually two LFOs under the hood,
        //where one is monophonic and one is polyphonic
        voiceParams.lfoParams[i] = lfoParams[i];
    }

    for (int i = 0; i < NUM_RANDOMS; i++)
    {
        randomLfoParams[i].setup(p, i);
        voiceParams.randomLfoParams[i] = randomLfoParams[i];
    }

    for (int i = 0; i < NUM_ENVELOPES; i++)
    {
        juce::String prefix = "Env " + std::to_string(i + 1);
        adsrParams[i].setup(p, prefix, i);
        voiceParams.adsrParams[i] = adsrParams[i];
    }

    for (int i = 0; i < NUM_MSEGS; i++)
    {
        msegParams[i].setup(p, i);
        voiceParams.msegParams[i] = msegParams[i];
    }

    for (int i = 0; i < NUM_MACROS; i++)
    {
        macroParams[i] = p.addExtParam("macro" + std::to_string(i + 1), "Macro " + std::to_string(i + 1),
                                       "Macro " + std::to_string(i + 1), "",
                                       {0.0f, 1.0f, 0.0f, 1.0f}, 0.0f,
                                       0.0f);
    }

    soloResonator = p.addIntParam("soloParameter", "Solo Parameter", "Solo", "",
                                  {-1.0f, NUM_RESONATOR_BANKS * NUM_RESONATORS, 1.0f, 1.0f}, -1.0f,
                                  0.0f, "resonator.solo");

    for(int i = 0; i < NUM_RESONATOR_BANKS; i++)
    {
        voiceParams.waveguideResonatorBankParams[i].soloResonator = soloResonator;
    }
}

void UIParams::setup(ResonariumProcessor& p)
{
    resonatorBankSelect = p.addIntParam("uiActiveResonatorBank", "Bank", "", "",
                                        {0, NUM_RESONATOR_BANKS - 1, 1.0f, 1.0f},
                                        0.0f, gin::SmoothingType::linear);

    lfoSelect = p.addIntParam("uiActiveLfo", "LFO", "", "",
                              {0.0f, NUM_LFOS - 1, 1.0f, 1.0f},
                              0.0f, gin::SmoothingType::linear);

    randomLfoSelect = p.addIntParam("uiActiveRandLfo", "RAND", "", "",
                                    {0.0f, NUM_LFOS - 1, 1.0f, 1.0f},
                                    0.0f, gin::SmoothingType::linear);

    msegSelect = p.addIntParam("uiActiveMSEG", "MSEG", "", "",
                               {0.0f, NUM_MSEGS - 1, 1.0f, 1.0f},
                               0.0f, gin::SmoothingType::linear);

    adsrSelect = p.addIntParam("uiActiveADSR", "ADSR", "", "",
                               {0.0f, NUM_ENVELOPES - 1, 1.0f, 1.0f},
                               0.0f, gin::SmoothingType::linear);

    modWindowSelect = p.addIntParam("uiActiveModWindow", "Mod Window", "", "",
                                    {0.0f, 2.0f, 1.0f, 1.0f},
                                    0.0f, gin::SmoothingType::linear);

    bypassResonators = p.addIntParam("uiBypassResonators", "BypassResonators", "", "",
                                     {0.0f, 1.0f, 1.0f, 1.0f},
                                     0.0f, 0.0f);
}
