#ifndef GLOBALSTATE_H
#define GLOBALSTATE_H

#include <JuceHeader.h>
#include "defines.h"
#include "dsp/Sampler.h"

/**
(A reference to) this struct is passed down to all components that require global state information,
such as global buffers, parameters, and modulation matrices.

We do not want low-level components to reach up the tree into the plugin processor to get important information.
Such behavior often induces circular dependencies, and it's generally bad practice.
*/
class GlobalState
{
public:
    GlobalState() = default;
    ~GlobalState() = default;
    gin::ModMatrix modMatrix;
    gin::ModSrcId modSrcPressure;
    gin::ModSrcId modSrcTimbre;
    gin::ModSrcId modSrcPitchbend;
    gin::ModSrcId modSrcNote;
    gin::ModSrcId modSrcVelocity;
    std::array<gin::ModSrcId, NUM_LFOS> modSrcMonoLFO;
    std::array<gin::ModSrcId, NUM_LFOS> modSrcPolyLFO;
    std::array<gin::ModSrcId, NUM_RANDOMS> modSrcMonoRND;
    std::array<gin::ModSrcId, NUM_RANDOMS> modSrcPolyRND;
    std::array<gin::ModSrcId, NUM_ENVELOPES> modSrcPolyENV;
    std::array<gin::ModSrcId, NUM_MSEGS> modSrcMonoMSEG;
    std::array<gin::ModSrcId, NUM_MSEGS> modSrcPolyMSEG;
    std::array<gin::ModSrcId, 119> modSrcCC;
    std::array<gin::ModSrcId, NUM_MACROS> modSrcMacro;
    Sampler sampler;
    juce::String samplePath = "";
    juce::String logPrefix = "";
    bool bypassResonators =  false;
    bool soloActive = false;
    int soloBankIndex = 0;
    int soloResonatorIndex = 0;
    bool polyFX = false;
    juce::AudioPlayHead* playHead;
    juce::AudioBuffer<float> extInputBuffer;
};


#endif //GLOBALSTATE_H
