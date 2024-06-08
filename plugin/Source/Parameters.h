#ifndef PARAMETERS_H
#define PARAMETERS_H

#include <JuceHeader.h>
#include "defines.h"
/**
* The Parameters file includes structs that manage the host-visible parameters of the synthesizer.
* The parameters are divided into structs for readability and compartmentalization.
*   (I'd rather keep the raw DSP code separate from the plugin/host interface bookkeeping code.)
* Functions that map parameter state to information strings, etc, are also included here.
* Since the structs contain only pointers, and parameters are not added or changed after instantiation,
* these structs are lightweight and can be passed around safely.
* Objects which require access to a parameter set are to be provided with the appropiate struct during initialization.
*/

class ResonariumProcessor;

/**
 * Parameters for the exciter module.
 */
struct ExciterParams
{
    gin::Parameter::Ptr attack, decay, sustain, release, level;

    void setup(ResonariumProcessor& p);
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

    ResonatorParams() : resonatorIndex(-1), bankIndex(-1), harmonic(nullptr), decayTime(nullptr), dispersion(nullptr),
                        decayFilterCutoff(nullptr), decayFilterType(nullptr), decayFilterResonance(nullptr),
                        decayFilterKeytrack(nullptr), gain(nullptr), testParameter(nullptr)
    {
    }

    ResonatorParams(int resonatorIndex, int bankIndex);

    void setup(ResonariumProcessor& p);
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

    ResonatorBankParams() : index(-1), resonatorParams{}, noteOffset(nullptr), couplingMode(nullptr),
                            outputGain(nullptr)
    {
    }

    ResonatorBankParams(int index);

    void setup(ResonariumProcessor& p);
};


#endif //PARAMETERS_H
