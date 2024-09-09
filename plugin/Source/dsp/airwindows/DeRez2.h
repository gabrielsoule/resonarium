/* ========================================
 *  DeRez2 - DeRez2.h
 *  Created 8/12/11 by SPIAdmin 
 *  Copyright (c) 2011 __MyCompanyName__, Airwindows uses the MIT license
 * ======================================== */

#pragma once

#include "FXBase.h"
#include <set>
#include <string>
#include <math.h>

class DeRez2 final : public AudioEffectX
{
public:
    enum {
        kParamA = 0,
        kParamB = 1,
        kParamC = 2,
        kParamD = 3,
      kNumParameters = 4
    }; //

    static constexpr int kNumPrograms = 0;
    static constexpr int kNumInputs = 2;
    static constexpr int kNumOutputs = 2;
    static constexpr unsigned long kUniqueId = 'eerz';    //Change this to what the AU identity is!
    
    DeRez2(audioMasterCallback audioMaster);
    ~DeRez2();
	void reset();
    bool getEffectName(char* name);                       // The plug-in name
    VstPlugCategory getPlugCategory();                    // The general category for the plug-in
    bool getProductString(char* text);                    // This is a unique plug-in string provided by Steinberg
    bool getVendorString(char* text);                     // Vendor info
    VstInt32 getVendorVersion();                          // Version number
    void processReplacing (float** inputs, float** outputs, VstInt32 sampleFrames);
    void processDoubleReplacing (double** inputs, double** outputs, VstInt32 sampleFrames);
    void getProgramName(char *name);                      // read the name from the host
    void setProgramName(char *name);                      // changes the name of the preset displayed in the host
	VstInt32 getChunk (void** data, bool isPreset);
	VstInt32 setChunk (void* data, VstInt32 byteSize, bool isPreset);
    float getParameter(VstInt32 index);                   // get the parameter value at the specified index
    void setParameter(VstInt32 index, float value);       // set the parameter at index to value
    void getParameterLabel(VstInt32 index, char *text);  // label for the parameter (eg dB)
    static void getParameterName(VstInt32 index, char *text);    // name of the parameter
    void getParameterDisplay(VstInt32 index, char *text); // text description of the current value
    VstInt32 canDo(char *text);
private:
    char _programName[kVstMaxProgNameLen + 1];
    std::set< std::string > _canDo;
    
	double lastSampleL;
	double heldSampleL;
	double lastDrySampleL;
	double lastOutputSampleL;

	double lastSampleR;
	double heldSampleR;
	double lastDrySampleR;
	double lastOutputSampleR;

	double position;
	double incrementA;
	double incrementB;
	
	uint32_t fpdL;
	uint32_t fpdR;
	//default stuff

    float A;
    float B;
    float C;
    float D;

};

