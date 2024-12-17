#ifndef PARAMETERS_H
#define PARAMETERS_H

#include <JuceHeader.h>
#include "defines.h"

/**
* The Parameters file includes structs that manage the host-visible parameters of the synthesizer.
* The parameters are divided into structs for readability and compartmentalization.
*   (I'd rather keep the raw DSP code separate from the plugin/host interface bookkeeping code, and this way,
*   all the parameter initialization is done in one file)
* Functions that map parameter state to information strings, etc, are also included here.
* Since the structs contain only pointers, and parameters are not added or changed after instantiation,
* these structs are lightweight and can be passed around safely.
* Objects which require access to a parameter set are to be provided with the appropiate struct during initialization.
*
* These classes should also contain any data that needs to be
* shared between the audio and UI threads, especially that which must
* be saved as part of a preset.
*/

class ResonariumProcessor;


/**
 * Parameters for a multi-mode filter.
 */
struct MultiFilterParams
{
    //since we expect anticipate instances of MultiFilterParams to exist,
    //we need to give each one a unique name for the host.
    juce::String prefix;
    gin::Parameter::Ptr
        type,
        frequency,
        resonance = nullptr;

    MultiFilterParams() = default;
    MultiFilterParams(ResonariumProcessor& p, juce::String prefix);
};

/**
 * Parameters for an ADSR envelope.
 */
struct ADSRParams
{
    //since we anticipate several ADSR instances to exist,
    //we need to give each one a unique name for the host.
    juce::String prefix;
    //the freely assignable ADSRs have indices; the built-in ones do not
    int index = -1;
    gin::Parameter::Ptr
        enabled,
        mode,
        attack,
        decay,
        sustain,
        release = nullptr;

    ADSRParams() = default;
    ADSRParams(ResonariumProcessor& p, juce::String prefix, int index);
};

/**
 * Parameters for a single resonator.
 */
struct ResonatorParams
{
    // "This is the resonatorIndex'th resonator in the bankIndex'th bank."
    // Since the parameters must have unique names for the host, we need to keep track of the indices.
    int resonatorIndex = -1;
    int bankIndex = -1;
    gin::Parameter::Ptr
        enabled,
        pitch,
        frequency,
        resonatorKeytrack,
        decayTime,
        dispersion,
        loopFilterCutoff,
        loopFilterPitchInSemis,
        loopFilterResonance, //for both SVF and BQD filter modes
        loopFilterMode, //for SVF filter mode only
        loopFilterType,
        loopFilterKeytrack, //bool y/n
        postFilterCutoff,
        postFilterPitchInSemis,
        postFilterResonance,
        postFilterMode,
        postFilterKeytrack, // bool y/n
        gain,
        testParameter = nullptr;

    ResonatorParams() = default;
    ResonatorParams(ResonariumProcessor& p, int resonatorIndex, int bankIndex);
};

/**
 * Parameters for a resonator bank. Contains several ResonatorParam structs.
 */
struct WaveguideResonatorBankParams
{
    int index = -1;
    std::array<ResonatorParams, NUM_RESONATORS> resonatorParams;
    gin::Parameter::Ptr
        soloResonator,
        //<--this parameter is the SAME parameter as the one that the synth uses, it should NOT be setup from scratch here
        noteOffset,
        useSemitones,
        couplingMode,
        inputGain,
        inputMix,
        outputGain,
        cascadeLevel,
        cascadeFilterCutoff,
        cascadeFilterResonance,
        cascadeFilterMode = nullptr;

    WaveguideResonatorBankParams() = default;
    WaveguideResonatorBankParams(ResonariumProcessor& p, int index);
};

struct ImpulseExciterParams
{
    int index = -1;
    MultiFilterParams filterParams;
    gin::Parameter::Ptr
        enabled,
        thickness,
        pickPosition,
        level = nullptr;

    ImpulseExciterParams() = default;
    ImpulseExciterParams(ResonariumProcessor& p, int index);
};

struct NoiseExciterParams
{
    int index = -1;
    MultiFilterParams filterParams;
    ADSRParams adsrParams;
    gin::Parameter::Ptr
        enabled,
        type,
        density,
        level = nullptr;

    NoiseExciterParams() = default;
    NoiseExciterParams(ResonariumProcessor& p, int index);
};

struct ImpulseTrainExciterParams
{
    int index = -1;
    MultiFilterParams filterParams;
    ADSRParams adsrParams;
    gin::Parameter::Ptr
        enabled,
        mode,
        rate,
        sync,
        entropy,
        character,
        level = nullptr;

    ImpulseTrainExciterParams() = default;
    ImpulseTrainExciterParams(ResonariumProcessor& p, int index);
};

struct ExternalInputExciterParams
{
    MultiFilterParams filterParams;
    gin::Parameter::Ptr
        enabled,
        gain,
        mix = nullptr;

    ExternalInputExciterParams() = default;
    ExternalInputExciterParams(ResonariumProcessor& p);
};

struct SampleExciterParams
{
    MultiFilterParams filterParams;
    ADSRParams adsrParams;
    gin::Parameter::Ptr
        enabled,
        mix,
        start,
        end,
        loop,
        level = nullptr;

    SampleExciterParams() = default;
    SampleExciterParams(ResonariumProcessor& p);
};

struct LFOParams
{
    int index = -1;
    gin::Parameter::Ptr
        enabled,
        sync,
        retrig,
        wave,
        rate,
        beat,
        depth,
        offset,
        phase,
        fade,
        delay,
        stereo = nullptr;

    LFOParams() = default;
    LFOParams(ResonariumProcessor& p, int index);
};

struct RandomLFOParams
{
    int index = -1;
    int seed = 42;
    gin::Parameter::Ptr
        enabled,
        mode,
        sync,
        rate,
        beat,
        depth,
        offset,
        smooth,
        jitter,
        chaos,
        stereo = nullptr;

    RandomLFOParams() = default;
    RandomLFOParams(ResonariumProcessor& p, int index);
};

struct MSEGParams
{
    int index = -1;
    //the msegData is loaded from disk;
    //furthermore, it needs to be shared between the UI and audio threads
    //good candidate for being shoved into a parameter struct
    std::shared_ptr<gin::MSEG::Data> msegData;
    gin::Parameter::Ptr
        enabled,
        sync,
        rate,
        beat,
        depth,
        offset,
        fade,
        phase,
        xgrid,
        ygrid,
        loop = nullptr;

    MSEGParams() = default;
    MSEGParams(ResonariumProcessor& p, int index);
};

struct ChorusParams
{
    gin::Parameter::Ptr
        enabled,
        rate,
        beat,
        sync,
        depth,
        delay,
        feedback,
        mix = nullptr;

    ChorusParams() = default;
    ChorusParams(ResonariumProcessor& p);
};

struct DelayParams
{
    gin::Parameter::Ptr
        enabled,
        timeL,
        timeR,
        beatL,
        beatR,
        pingPongAmount,
        syncL,
        syncR,
        feedback,
        lock,
        mix = nullptr;

    DelayParams() = default;
    DelayParams(ResonariumProcessor& p);
};

struct DistortionParams
{
    gin::Parameter::Ptr
        enabled,
        distortionMode,
        drive,
        outputGain,
        mix,
        prePostFilter,
        cutoff,
        filterMode,
        resonance = nullptr;

    DistortionParams() = default;
    DistortionParams(ResonariumProcessor& p);
};

struct MultiAmpParams
{
    gin::Parameter::Ptr
        enabled,
        mode,
        paramA,
        paramB,
        paramC,
        paramD = nullptr;

    MultiAmpParams() = default;
    MultiAmpParams(ResonariumProcessor& p);
};

struct PhaserParams
{
    gin::Parameter::Ptr
        enabled,
        rate,
        sync,
        beat,
        depth,
        centreFrequency,
        feedback,
        mix = nullptr;

    PhaserParams() = default;
    PhaserParams(ResonariumProcessor& p);
};

struct CompressorParams
{
    gin::Parameter::Ptr
        enabled,
        threshold,
        ratio,
        attack,
        release = nullptr;

    CompressorParams() = default;
    CompressorParams(ResonariumProcessor& p);
};

struct ReverbParams
{
    gin::Parameter::Ptr
        enabled,
        dampingFreq,
        density,
        bandwidthFreq,
        decay,
        predelay,
        size,
        gain,
        earlyMix,
        mix = nullptr;

    ReverbParams() = default;
    ReverbParams(ResonariumProcessor& p);
};

struct SVFParams
{
    juce::String name;
    gin::Parameter::Ptr
        enabled,
        mode,
        cutoff,
        resonance = nullptr;

    SVFParams() = default;
    SVFParams(ResonariumProcessor& p, juce::String name);
};

struct EffectChainParams
{
    ChorusParams chorusParams;
    DelayParams delayParams;
    DistortionParams distortionParams;
    MultiAmpParams multiAmpParams;
    SVFParams filterParams[2];
    CompressorParams compressorParams;
    PhaserParams phaserParams;
    ReverbParams reverbParams;

    gin::Parameter::Ptr
        poly,
        gain;

    EffectChainParams() = default;
    EffectChainParams(ResonariumProcessor& p);
};

/**
 * This parameter struct encapsulates everything needed to set up a resonator voice.
 * The setup function should only ever be called once, during initialization.
 * However, once set up, the struct is safe to duplicate and pass around.
 * When the plugin is loaded, each ResonatorVoice will copy the contents of this struct to
 * the appropriate child DSP components.
 * These structs are super cheap to copy, plus we only do this once at setup,
 * so it's not a big deal to pass-by-value.
 *
 * It's important to note that there is only one VoiceParams object per plugin instance,
 * which is shared by all voices.
 */
struct VoiceParams
{
    std::array<ImpulseExciterParams, NUM_IMPULSE_EXCITERS> impulseExciterParams;
    std::array<NoiseExciterParams, NUM_NOISE_EXCITERS> noiseExciterParams;
    std::array<ImpulseTrainExciterParams, NUM_IMPULSE_TRAIN_EXCITERS> impulseTrainExciterParams;
    ExternalInputExciterParams externalInputExciterParams;
    SampleExciterParams sampleExciterParams;

    std::array<WaveguideResonatorBankParams, NUM_RESONATOR_BANKS> waveguideResonatorBankParams;

    std::array<LFOParams, NUM_LFOS> lfoParams;
    std::array<RandomLFOParams, NUM_RANDOMS> randomLfoParams;
    std::array<ADSRParams, NUM_ENVELOPES> adsrParams;
    std::array<MSEGParams, NUM_MSEGS> msegParams;

    EffectChainParams effectChainParams;

    VoiceParams() = default;
    VoiceParams(ResonariumProcessor& p);
};

struct GlobalParams
{
    gin::Parameter::Ptr numVoices,
                        stereoResonators,
                        polyEffectChain,
                        gain = nullptr;

    GlobalParams() = default;
    GlobalParams(ResonariumProcessor& p);
};

/**
 * Parameters for the synthesizer as a whole. These parameters by definition
 * must be monophonic, as they apply to the entire synthesizer.
 */
struct SynthParams
{
    VoiceParams voiceParams;
    std::array<LFOParams, NUM_LFOS> lfoParams;
    std::array<RandomLFOParams, NUM_LFOS> randomLfoParams;
    std::array<ADSRParams, NUM_ENVELOPES> adsrParams;
    std::array<MSEGParams, NUM_MSEGS> msegParams;
    std::array<gin::Parameter::Ptr, NUM_MACROS> macroParams;
    EffectChainParams effectChainParams;
    GlobalParams globalParams;
    gin::Parameter::Ptr soloResonator;

    SynthParams() = default;
    SynthParams(ResonariumProcessor& p);
};

struct UIParams
{
    gin::Parameter::Ptr resonatorBankSelect;
    gin::Parameter::Ptr lfoSelect;
    gin::Parameter::Ptr randomLfoSelect;
    gin::Parameter::Ptr msegSelect;
    gin::Parameter::Ptr adsrSelect;
    gin::Parameter::Ptr modWindowSelect;
    gin::Parameter::Ptr bypassResonators;

    UIParams() = default;
    UIParams(ResonariumProcessor& p);
};


#endif //PARAMETERS_H
