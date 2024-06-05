#ifndef PARAMETERS_H
#define PARAMETERS_H

#include <JuceHeader.h>
#include "defines.h"
/**
* The Parameters file includes structs that manage the host-visible parameters of the synthesizer.
* The parameters are divided into structs for ease of use and compartmentalization.
*   (I'd rather keep the raw DSP code separate from the plugin/host interface bookkeeping code.)
* Since the structs contain only pointers, they are lightweight and can be passed around easily.
* Objects which require access to a parameter set will be provided with the appropiate struct during initialization.
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
    int index;
    gin::Parameter::Ptr
        harmonic,
        decayTime,
        decayFilterCutoff,
        dispersion,
        decayFilterType,
        decayFilterResonance,
        decayFilterKeytrack,
        gain;

    void setup(ResonariumProcessor& p);
};

/**
 * Parameters for a resonator bank. Contains several ResonatorParam structs.
 */
struct ResonatorBankParams
{
    int index;
    ResonatorParams resonatorParams[NUM_RESONATORS];
    gin::Parameter::Ptr noteOffset, couplingMode, outputGain;

    void setup(ResonariumProcessor& p);
};


#endif //PARAMETERS_H
