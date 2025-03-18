#pragma once

#include <cmath>
#include <functional>
#include <cstring>
#include <juce_core/juce_core.h>

// Define M_PI for Windows builds
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define VstInt32                int32_t
#define AudioEffect             FXBase
#define AudioEffectX            FXBase
#define audioMasterCallback     FXBaseCallback
#define VstPlugCategory         int
#define kPlugCategEffect        1
#define kVstMaxProgNameLen      32
#define kVstMaxParamStrLen      32
#define kVstMaxProductStrLen    32
#define kVstMaxVendorStrLen     32
#define vst_strncpy             strncpy

inline void float2string (float f, char* text, int len)
{
    int decimals = 0;
    if (std::fabs (f) >= 10.0f)
        decimals = 1;
    else if (std::fabs (f) > 1.0f)
        decimals = 2;
    else
        decimals = 3;

    juce::String str (f, decimals);
    str.copyToUTF8 (text, (size_t)len);
}

inline void int2string (float i, char* text, int len)
{
    juce::String str (i);
    str.copyToUTF8 (text, (size_t)len);
}

inline void dB2string (float value, char* text, int maxLen)
{
    if (value <= 0)
        vst_strncpy (text, "-oo", (size_t) maxLen);
    else
        float2string (20.0f * log10 (value), text, maxLen);
}

//==============================================================================
class FXBaseCallback
{
public:
    FXBaseCallback (std::function<double()> cb_) : cb (cb_) {}

    double getSampleRate()
    {
        return cb();
    }

    std::function<double()> cb;
};

//==============================================================================
class FXBase
{
public:
    //==============================================================================
    FXBase (FXBaseCallback c, int numPrograms_, int numParams_)
        : numPrograms (numPrograms_), numParams (numParams_), callback (c)
    {
    }

    virtual ~FXBase() = default;

    int getNumInputs()                  { return numInputs;     }
    int getNumOutputs()                 { return numOutputs;    }
    int getNumParameters()              { return numParams;     }

    //==============================================================================
    virtual bool getEffectName(char* name)                        = 0;
    virtual VstPlugCategory getPlugCategory()                     = 0;
    virtual bool getProductString(char* text)                     = 0;
    virtual bool getVendorString(char* text)                      = 0;
    virtual VstInt32 getVendorVersion()                           = 0;
    virtual void processReplacing (float** inputs, float** outputs, VstInt32 sampleFrames) = 0;
    virtual void processDoubleReplacing (double** inputs, double** outputs, VstInt32 sampleFrames) = 0;
    virtual void getProgramName(char* name)                       = 0;
    virtual void setProgramName(char* name)                       = 0;
    virtual VstInt32 getChunk (void** data, bool isPreset)                          { juce::ignoreUnused (data, isPreset); return 0; }
    virtual VstInt32 setChunk (void* data, VstInt32 byteSize, bool isPreset)        { juce::ignoreUnused (data, byteSize, isPreset); return 0; }
    virtual float getParameter(VstInt32 index)                                      { juce::ignoreUnused (index); return 0; }
    virtual void setParameter(VstInt32 index, float value)                          { juce::ignoreUnused (index, value); }
    virtual void getParameterLabel(VstInt32 index, char* text)                      { juce::ignoreUnused (index, text); }
    static void getParameterName(VstInt32 index, char* text)                       { juce::ignoreUnused (index, text); }
    virtual void getParameterDisplay(VstInt32 index, char* text)                    { juce::ignoreUnused (index, text); }
    virtual VstInt32 canDo(char *text)                            = 0;

protected:
    //==============================================================================
    void setNumInputs (int numIn)       { numInputs = numIn;    }
    void setNumOutputs (int numOut)     { numOutputs = numOut;  }
    void setUniqueID (int)              {}
    void canProcessReplacing()          {}
    void canDoubleReplacing()           {}
    void programsAreChunks (bool)       {}

    int numInputs = 0, numOutputs = 0, numPrograms = 0, numParams = 0;

    FXBaseCallback callback;

    double getSampleRate()              { return callback.getSampleRate(); }
};
