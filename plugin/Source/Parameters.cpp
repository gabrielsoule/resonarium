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
    case 2: return "HP";
    case 3: return "BP";
    case 4: return "NT";
    case 5: return "AP";
    default:
        jassertfalse;
        return {};
    }
}

static juce::String resonatorFilterTypeTextFunction(const gin::Parameter&, float v)
{
    switch (int(v))
    {
    case 0: return "EKS";
    case 1: return "BQD";
    case 2: return "SVF";
    case 3: return "EQ3";
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

void MultiFilterParams::setup(ResonariumProcessor& p, juce::String prefix)
{
    this->prefix = prefix;

    type = p.addExtParam(prefix + "filterType", prefix + " Filter Type", "Filter", "",
                         {0.0f, 5.0f, 1.0f, 1.0f},
                         0.0f, 0.0f, filterTextFunction);
    frequency = p.addExtParam(prefix + "frequency", prefix + " Frequency ", "Freq", "Hz",
                              {20.0f, 20000.0f, 0.0f, 0.4f},
                              1000.0f, 0.0f);
    resonance = p.addExtParam(prefix + "_resonance", prefix + " Resonance", "Res", "",
                              {0.01f, 100.0f, 0.0f, 0.4f},
                              0.707f, 0.0f);
}

void ADSRParams::setup(ResonariumProcessor& p, juce::String prefix)
{
    this->prefix = prefix;

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
    jassert(resonatorIndex >= 0 && resonatorIndex < NUM_WAVEGUIDE_RESONATORS);
    jassert(bankIndex >= 0 && bankIndex < NUM_WAVEGUIDE_RESONATOR_BANKS);

    this->resonatorIndex = resonatorIndex;
    this->bankIndex = bankIndex;

    juce::String suffix = " wb" + std::to_string(bankIndex) + "r" + std::to_string(resonatorIndex);

    enabled = p.addExtParam("enabled" + suffix, "Enabled" + suffix, "On/Off", " ",
                            {0.0, 1.0, 1.0, 1.0f}, 0.0f,
                            0.0f);

    harmonicInSemitones = p.addExtParam("pitchOffsetSemis" + suffix, "Pitch Offset" + suffix, "Pitch", " st",
                                        {-60.0f, 60.0f, 0.01f, 1.0f}, 0.0f,
                                        0.0f);

    harmonicMultiplier = p.addExtParam("harmonicMultiplier" + suffix, "Harmonic Multiplier" + suffix, "Mult.", "",
                                       {0.0f, 20.0f, 0.01f, 0.4f}, 1.0f,
                                       0.0f);

    decayTime = p.addExtParam("decayTime" + suffix, "Decay Time" + suffix, "Decay", "s",
                              {0.0f, 60.0f, 0.0f, 0.2f}, 3.0f,
                              0.0f);

    dispersion = p.addExtParam("dispersion" + suffix, "Dispersion" + suffix, "Disp.", "%",
                               {0.0f, 100.0f, 0.0f, 1.0f}, 0.0f,
                               0.0f);
    dispersion->conversionFunction = [](const float x) { return x / 100.0f; };

    // EKS, BQD, SVF, EQ3
    decayFilterType = p.addExtParam("decayFilterType" + suffix, "Filter Type" + suffix, "Filter", "",
                                    {0.0, 3.0, 1.0, 1.0f}, 0.0f,
                                    0.0f, resonatorFilterTypeTextFunction);

    // LP, HP, BP, NOTCH, AP
    biquadFilterType = p.addExtParam("biquadFilterType" + suffix, "Biquad Type" + suffix, "Biquad Type", "",
                                     {0.0f, 5.0f, 1.0f, 1.0f},
                                     0.0f, 0.0f, filterTextFunction);

    decayFilterCutoff = p.addExtParam("decayFilterCutoff" + suffix, "Filter Cutoff" + suffix, "Cutoff", "Hz",
                                      {20.0f, 20000.0, 0.0f, 1.0f}, 1000.0f,
                                      0.0f);

    decayFilterResonance = p.addExtParam("decayFilterResonance" + suffix, "Resonance" + suffix, "Res", "",
                                         {0.01f, 100.0f, 0.0f, 0.2f}, 0.707f,
                                         0.0f);

    decayFilterKeytrack = p.addExtParam("filterKeytrack" + suffix, "Keytrack" + suffix, "Key Track", "%",
                                        {0.0f, 100.0f, 0.0f, 1.0f}, 0.0f,
                                        0.0f);

    eksFilterBrightness = p.addExtParam("eksBrightness" + suffix, "Brightness" + suffix, "Brightness", "",
                                        {0.0f, 1.0f, 0.0f, 1.0f}, 0.5f,
                                        0.0f);

    svfFilterMode = p.addExtParam("svfMode" + suffix, "Mode" + suffix, "Mode", "",
                                  {-1.0, 1.0f, 0.0, 1.0f}, 0.0f,
                                  0.0f);

    gain = p.addExtParam("gain" + suffix, "Gain" + suffix, "Gain", "dB",
                         {-100.0, 0.0, 0.0, 4.0f}, 0.0f,
                         0.0f);
    gain->conversionFunction = [](const float x) { return juce::Decibels::decibelsToGain(x); };

    testParameter = p.addExtParam("testParameter" + suffix, "Test Parameter" + suffix, "Test", "",
                                  {0.0, 1.0, 0.0, 1.0f}, 0.0f,
                                  0.0f);
}

void WaveguideResonatorBankParams::setup(ResonariumProcessor& p, int index)
{
    jassert(index >= 0 && index < NUM_WAVEGUIDE_RESONATOR_BANKS);
    this->index = index;
    for (int i = 0; i < NUM_WAVEGUIDE_RESONATORS; i++)
    {
        resonatorParams[i].setup(p, i, index);
    }

    juce::String suffix = " wb" + std::to_string(index);

    noteOffset = p.addExtParam("noteOffset" + suffix, "Note Offset" + suffix, "Note", "semitones",
                               {-24.0f, 24.0f, 0.0f, 1.0f}, 0.0f,
                               gin::SmoothingType::linear);

    couplingMode = p.addExtParam("couplingMode" + suffix, "Coupling Mode" + suffix, "Coupling", "",
                                 {0.0, 2.0, 1.0, 1.0f}, 0.0f,
                                 gin::SmoothingType::linear, couplingModeTextFunction);

    outputGain = p.addExtParam("outputGain" + suffix, "Output Gain" + suffix, "Gain", "dB",
                               {-100.0, 0.0, 0.0, 4.0f}, 0.0f,
                               gin::SmoothingType::linear);
    outputGain->conversionFunction = [](const float x) { return juce::Decibels::decibelsToGain(x); };
}

void ModalResonatorBankParams::setup(ResonariumProcessor& p, int index)
{
    jassert(index >= 0 && index < NUM_WAVEGUIDE_RESONATOR_BANKS);
    this->index = index;

    juce::String bankSuffix = " mb" + std::to_string(index);

    for (int i = 0; i < NUM_MODAL_RESONATORS; i++)
    {
        juce::String resonatorSuffix = bankSuffix + "r" + juce::String(i);
        enabled[i] = p.addExtParam("enabled" + resonatorSuffix, "Enabled" + resonatorSuffix, "On/Off", " ",
                                   {0.0, 1.0, 1.0, 1.0f}, 0.0f,
                                   gin::SmoothingType::linear);
        harmonicInSemitones[i] = p.addExtParam("pitchOffsetSemis" + resonatorSuffix, "Pitch Offset" + resonatorSuffix,
                                               "Pitch", " st",
                                               {-60.0f, 60.0f, 0.01f, 1.0f}, 0.0f,
                                               0.0f);
        decay[i] = p.addExtParam("decay" + resonatorSuffix, "Decay " + resonatorSuffix, "Decay", "",
                                 {0.0f, 1.0f, 0.0f, 1.0f}, 0.5f,
                                 gin::SmoothingType::linear);
        decay[i]->conversionFunction = [](const float x) { return x * 2.0f; };

        gain[i] = p.addExtParam("gain" + resonatorSuffix, "Gain" + resonatorSuffix, "Gain", "dB",
                                {-100.0f, 12.0f, 0.0f, 1.0f}, 0.0f,
                                0.0f);
        gain[i]->conversionFunction = [](const float x) { return juce::Decibels::decibelsToGain(x); };
    }
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

    filterParams.setup(p, prefix);
    adsrParams.setup(p, prefix);
}

void ImpulseTrainExciterParams::setup(ResonariumProcessor& p, int index)
{
    this->index = index;
    juce::String prefix = "impTrainExciter" + std::to_string(index);

    this->enabled = p.addExtParam(prefix + "enabled", prefix + " Enabled", "On/Off", "",
                                  {0.0f, 1.0f, 1.0f, 1.0f}, 0.0f,
                                  0.0f);

    this->mode = p.addExtParam(prefix + "mode", prefix + " Mode", "Mode", "",
                               {0.0f, 3.0f, 1.0f, 1.0f}, 0.0f,
                               0.0f, impulseTrainExciterModeTextFunction);

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


    filterParams.setup(p, prefix);
    adsrParams.setup(p, prefix);
}

void LFOParams::setup(ResonariumProcessor& p, int index)
{
    this->index = index;
    juce::String prefix = "lfo" + std::to_string(index);
    auto notes = gin::NoteDuration::getNoteDurations();

    enabled = p.addExtParam(prefix + "enable", prefix + "Enable", "Enable", "",
                            {0.0f, 1.0f, 1.0f, 1.0f}, 0.0f,
                            0.0f, enableTextFunction);

    sync = p.addIntParam(prefix + "sync", prefix + "Sync", "Sync", "",
                         {0.0f, 1.0f, 1.0f, 1.0f}, 0.0f,
                         0.0f, enableTextFunction);

    retrig = p.addIntParam(prefix + "retrig", prefix + "Retrig", "Retrig", "",
                           {0.0f, 1.0f, 1.0f, 1.0f}, 1.0f,
                           0.0f, enableTextFunction);

    wave = p.addIntParam(prefix + "wave", prefix + "Wave", "Wave", "",
                         {1.0f, 17.0f, 1.0f, 1.0f}, 1.0f,
                         0.0f, lfoTextFunction);

    rate = p.addExtParam(prefix + "rate", prefix + "Rate", "Rate", "Hz",
                         {0.0f, 50.0f, 0.0f, 0.3f},
                         1.0f, 0.0f);

    beat = p.addIntParam(prefix + "beat", prefix + "Beat", "Beat", "",
                         {0.0f, float(notes.size() - 1), 1.0f, 1.0f},
                         13.0f, 0.0f, durationTextFunction);

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
                            0.0f, enableTextFunction);

    mode = p.addIntParam(prefix + "mode", prefix + "Mode", "Mode", "",
                         {0.0f, 2.0f, 1.0f, 1.0f}, 0.0f,
                         0.0f, randomLFOModeTextFunction);

    sync = p.addIntParam(prefix + "sync", prefix + "Sync", "Sync", "",
                         {0.0f, 1.0f, 1.0f, 1.0f}, 0.0f,
                         0.0f, enableTextFunction);

    rate = p.addExtParam(prefix + "rate", prefix + "Rate", "Rate", "Hz",
                         {0.0f, 50.0f, 0.0f, 0.3f},
                         1.0f, 0.0f);

    beat = p.addIntParam(prefix + "beat", prefix + "Beat", "Beat", "",
                         {0.0f, float(notes.size() - 1), 1.0f, 1.0f},
                         13.0f, 0.0f, durationTextFunction);

    depth = p.addExtParam(prefix + "depth", prefix + "Depth", "Depth", "",
                          {-1.0f, 1.0f, 0.0f, 1.0f},
                          1.0f, 0.0f);

    offset = p.addExtParam(prefix + "offset", prefix + "Offset", "Offset", "",
                           {-1.0f, 1.0f, 0.0f, 1.0f},
                           0.0f, 0.0f);

    smooth = p.addExtParam(prefix + "smooth", prefix + "Smooth", "Smooth", "%",
                           {0.0f, 1.0f, 0.01f, 1.0f},
                           0.0f, 0.0f);

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
    enabled = p.addExtParam(prefix + "enable", "MSEG" + std::to_string(index) + "Enable", "Enable", "",
                            {0.0f, 1.0f, 1.0f, 1.0f}, 0.0f,
                            0.0f, enableTextFunction);

    sync = p.addIntParam(prefix + "sync", "MSEG" + std::to_string(index) + "Sync", "Sync", "",
                         {0.0f, 1.0f, 1.0f, 1.0f}, 0.0f,
                         0.0f, enableTextFunction);

    rate = p.addExtParam(prefix + "rate", "MSEG" + std::to_string(index) + "Rate", "Rate", "Hz",
                         {0.0f, 50.0f, 0.0f, 0.3f},
                         10.0f, 0.0f);

    beat = p.addIntParam(prefix + "beat", "MSEG" + std::to_string(index) + "Beat", "Beat", "",
                         {0.0f, 13.0f, 1.0f, 1.0f},
                         13.0f, 0.0f, durationTextFunction);

    depth = p.addExtParam(prefix + "depth", "MSEG" + std::to_string(index) + "Depth", "Depth", "",
                          {-1.0f, 1.0f, 0.0f, 1.0f},
                          1.0f, 0.0f);

    offset = p.addExtParam(prefix + "offset", "MSEG" + std::to_string(index) + "Offset", "Offset", "",
                           {-1.0f, 1.0f, 0.0f, 1.0f},
                           0.0f, 0.0f);

    phase = p.addExtParam(prefix + "phase", "MSEG" + std::to_string(index) + "Phase", "Phase", "",
                          {-1.0f, 1.0f, 0.0f, 1.0f},
                          0.0f, 0.0f);

    xgrid = p.addExtParam(prefix + "xgrid", "MSEG" + std::to_string(index) + "XGrid", "XGrid", "",
                          {2.0f, 32.0f, 1.0f, 1.0f},
                          2.0f, 0.0f);

    ygrid = p.addExtParam(prefix + "ygrid", "MSEG" + std::to_string(index) + "YGrid", "YGrid", "",
                          {2.0f, 32.0f, 1.0f, 1.0f},
                          2.0f, 0.0f);

    loop = p.addIntParam(prefix + "loop", "MSEG" + std::to_string(index) + "Loop", "Loop", "",
                         {0.0f, 1.0f, 1.0f, 1.0f}, 0.0f,
                         0.0f, enableTextFunction);
}

void VoiceParams::setup(ResonariumProcessor& p)
{
    for (int i = 0; i < NUM_MODAL_RESONATOR_BANKS; i++)
    {
        modalResonatorBankParams[i].setup(p, i);
    }
    for (int i = 0; i < NUM_WAVEGUIDE_RESONATOR_BANKS; i++)
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

    //Do NOT set up LFO params; these are set later by the enclosing synth
}

void SynthParams::setup(ResonariumProcessor& p)
{
    voiceParams.setup(p);
    for (int i = 0; i < NUM_LFOS; i++)
    {
        lfoParams[i].setup(p, i);
        randomLfoParams[i].setup(p, i);

        //ensure that the same parameters are used for the synth-level monophonic LFOs
        //and the voice-level polyphonic LFOs
        //the result is that each LFO is actually two LFOs under the hood,
        //where one is monophonic and one is polyphonic
        voiceParams.lfoParams[i] = lfoParams[i];
        voiceParams.randomLfoParams[i] = randomLfoParams[i];
    }

    for(int i = 0; i < NUM_MSEGS; i++)
    {
        msegParams[i].setup(p, i);
    }
}

void UIParams::setup(ResonariumProcessor& p)
{
    resonatorBankSelect = p.addIntParam("uiActiveResonatorBank", "Bank", "", "",
                                        {0, NUM_MODAL_RESONATOR_BANKS + NUM_WAVEGUIDE_RESONATOR_BANKS - 1, 1.0f, 1.0f},
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

    bypassResonators = p.addIntParam("uiBypassResonators", "BypassResonators", "", "",
                                     {0.0f, 1.0f, 1.0f, 1.0f},
                                     0.0f, 0.0f);
}
