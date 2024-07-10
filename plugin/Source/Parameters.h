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
        type = nullptr,
        frequency = nullptr,
        resonance = nullptr;

    MultiFilterParams() = default;

    void setup(ResonariumProcessor& p, juce::String suffix);
};

/**
 * Parameters for an ADSR envelope.
 */
struct ADSRParams
{
    //since we anticipate several instances of MultiFilterParams to exist,
    //we need to give each one a unique name for the host.
    juce::String prefix;
    gin::Parameter::Ptr
        mode,
        attack,
        decay,
        sustain,
        release;

    ADSRParams() = default;

    void setup(ResonariumProcessor& p, juce::String prefix);
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
        harmonicInSemitones,
        harmonicMultiplier,
        decayTime,
        dispersion,
        decayFilterType, //EKS, BQD, SVF, EQ3
        biquadFilterType, //LP, HP, BP, NOTCH, AP
        decayFilterCutoff,
        decayFilterResonance, //for both SVF and BQD filter modes
        decayFilterKeytrack, //bool y/n
        eksFilterBrightness, //for EKS filter mode only
        svfFilterMode, //for SVF filter mode only
        gain,
        testParameter;

    ResonatorParams() = default;

    void setup(ResonariumProcessor& p, int resonatorIndex, int bankIndex);
};

/**
 * Parameters for a resonator bank. Contains several ResonatorParam structs.
 */
struct WaveguideResonatorBankParams
{
    int index = -1;
    ResonatorParams resonatorParams[NUM_WAVEGUIDE_RESONATORS];
    gin::Parameter::Ptr
        noteOffset,
        couplingMode,
        outputGain;

    WaveguideResonatorBankParams() = default;

    void setup(ResonariumProcessor& p, int index);
};

struct ModalResonatorBankParams
{
    int index = -1;
    gin::Parameter::Ptr enabled[NUM_MODAL_RESONATORS];
    gin::Parameter::Ptr harmonicMultiplier[NUM_MODAL_RESONATORS];
    gin::Parameter::Ptr harmonicInSemitones[NUM_MODAL_RESONATORS];
    gin::Parameter::Ptr decay[NUM_MODAL_RESONATORS];
    gin::Parameter::Ptr gain[NUM_MODAL_RESONATORS];

    ModalResonatorBankParams() = default;

    void setup(ResonariumProcessor& p, int index);
};

struct ImpulseExciterParams
{
    int index;
    MultiFilterParams filterParams;
    gin::Parameter::Ptr
        enabled,
        thickness,
        pickPosition,
        level;

    ImpulseExciterParams() = default;

    void setup(ResonariumProcessor& p, int index);
};

struct NoiseExciterParams
{
    int index;
    MultiFilterParams filterParams;
    ADSRParams adsrParams;
    gin::Parameter::Ptr
        enabled,
        type,
        density,
        level;

    NoiseExciterParams() = default;

    void setup(ResonariumProcessor& p, int index);
};

struct ImpulseTrainExciterParams
{
    int index;
    MultiFilterParams filterParams;
    ADSRParams adsrParams;
    gin::Parameter::Ptr
        enabled,
        mode,
        speed,
        sync,
        entropy,
        character,
        level;

    ImpulseTrainExciterParams() = default;

    void setup(ResonariumProcessor& p, int index);
};

struct LFOParams
{
    int index;
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
        stereo;

    LFOParams() = default;

    void setup(ResonariumProcessor& p, int index);
};

struct RandomLFOParams
{
    int index;
    gin::Parameter::Ptr
        enabled,
        sync,
        rate,
        beat,
        depth,
        offset,
        smooth,
        jitter,
        stereo;

    RandomLFOParams() = default;

    void setup(ResonariumProcessor& p, int index);
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
    WaveguideResonatorBankParams waveguideResonatorBankParams[NUM_WAVEGUIDE_RESONATOR_BANKS];
    ModalResonatorBankParams modalResonatorBankParams[NUM_MODAL_RESONATOR_BANKS];
    ImpulseExciterParams impulseExciterParams[NUM_IMPULSE_EXCITERS];
    NoiseExciterParams noiseExciterParams[NUM_NOISE_EXCITERS];
    ImpulseTrainExciterParams impulseTrainExciterParams[NUM_IMPULSE_TRAIN_EXCITERS];
    LFOParams lfoParams[NUM_LFOS];

    VoiceParams() = default;

    void setup(ResonariumProcessor& p);
};

/**
 * Parameters for the synthesizer as a whole. These parameters by definition
 * must be monophonic, as they apply to the entire synthesizer.
 */
struct SynthParams
{
    VoiceParams voiceParams;
    LFOParams lfoParams[NUM_LFOS];
    RandomLFOParams randomLfoParams[NUM_LFOS];

    SynthParams() = default;

    void setup(ResonariumProcessor& p);
};

struct UIParams
{
    gin::Parameter::Ptr resonatorBankSelect;
    gin::Parameter::Ptr lfoSelect;
    gin::Parameter::Ptr randomLfoSelect;

    UIParams() = default;

    void setup(ResonariumProcessor& p);
};


#endif //PARAMETERS_H
