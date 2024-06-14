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
        harmonic,
        decayTime,
        dispersion,
        decayFilterCutoff,
        decayFilterType,
        decayFilterResonance,
        decayFilterKeytrack,
        gain,
        testParameter;

    ResonatorParams() = default;

    void setup(ResonariumProcessor& p, int resonatorIndex, int bankIndex);
};

/**
 * Parameters for a resonator bank. Contains several ResonatorParam structs.
 */
struct ResonatorBankParams
{
    int index = -1;
    ResonatorParams resonatorParams[NUM_RESONATORS];
    gin::Parameter::Ptr
        noteOffset,
        couplingMode,
        outputGain;

    ResonatorBankParams() = default;

    void setup(ResonariumProcessor& p, int index);
};

struct ImpulseExciterParams
{
    int index;
    MultiFilterParams filterParams;
    gin::Parameter::Ptr
        thickness,
        pickPosition,
        gain;

    ImpulseExciterParams() = default;

    void setup(ResonariumProcessor& p, int index);
};

struct NoiseExciterParams
{
    int index;
    MultiFilterParams filterParams;
    ADSRParams adsrParams;
    gin::Parameter::Ptr
        type,
        density,
        gain;

    NoiseExciterParams() = default;

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
 */
struct VoiceParams
{
    ResonatorBankParams resonatorBankParams[NUM_RESONATOR_BANKS];
    ImpulseExciterParams impulseExciterParams[NUM_IMPULSE_EXCITERS];
    NoiseExciterParams noiseExciterParams[NUM_NOISE_EXCITERS];

    void setup(ResonariumProcessor& p);
};


#endif //PARAMETERS_H
