/* ========================================
 *  BigAmp - BigAmp.h
 *  Created 8/12/11 by SPIAdmin 
 *  Copyright (c) 2011 __MyCompanyName__, Airwindows uses the MIT license
 * ======================================== */

#pragma once

#include "FXBase.h"
#include <set>
#include <string>
#include <math.h>

class BigAmp final :
    public AudioEffectX 
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
	static constexpr unsigned long kUniqueId = 'biga';    //Change this to what the AU identity is!

    BigAmp(audioMasterCallback audioMaster);
    ~BigAmp();
	void reset();
    virtual bool getEffectName(char* name);                       // The plug-in name
    virtual VstPlugCategory getPlugCategory();                    // The general category for the plug-in
    virtual bool getProductString(char* text);                    // This is a unique plug-in string provided by Steinberg
    virtual bool getVendorString(char* text);                     // Vendor info
    virtual VstInt32 getVendorVersion();                          // Version number
    virtual void processReplacing (float** inputs, float** outputs, VstInt32 sampleFrames);
    virtual void processDoubleReplacing (double** inputs, double** outputs, VstInt32 sampleFrames);
    virtual void getProgramName(char *name);                      // read the name from the host
    virtual void setProgramName(char *name);                      // changes the name of the preset displayed in the host
	virtual VstInt32 getChunk (void** data, bool isPreset);
	virtual VstInt32 setChunk (void* data, VstInt32 byteSize, bool isPreset);
    virtual float getParameter(VstInt32 index);                   // get the parameter value at the specified index
    virtual void setParameter(VstInt32 index, float value);       // set the parameter at index to value
    virtual void getParameterLabel(VstInt32 index, char *text);  // label for the parameter (eg dB)
    static void getParameterName(VstInt32 index, char *text);    // name of the parameter
    virtual void getParameterDisplay(VstInt32 index, char *text); // text description of the current value    
    virtual VstInt32 canDo(char *text);
private:
    char _programName[kVstMaxProgNameLen + 1];
    std::set< std::string > _canDo;
    
	double lastSampleL;
	double storeSampleL;
	double lastSlewL;
	double iirSampleAL;
	double iirSampleBL;
	double iirSampleCL;
	double iirSampleDL;
	double iirSampleEL;
	double iirSampleFL;
	double iirSampleGL;
	double iirSampleHL;
	double iirSampleIL;
	double iirSampleJL;
	double OddL[257];
	double EvenL[257]; //amp
	
	double lastSampleR;
	double storeSampleR;
	double lastSlewR;
	double iirSampleAR;
	double iirSampleBR;
	double iirSampleCR;
	double iirSampleDR;
	double iirSampleER;
	double iirSampleFR;
	double iirSampleGR;
	double iirSampleHR;
	double iirSampleIR;
	double iirSampleJR;
	double OddR[257];
	double EvenR[257];
	
	bool flip;
	int count; //amp
	
	double bL[90];
	double lastCabSampleL;
	double smoothCabAL;
	double smoothCabBL; //cab
	
	double bR[90];
	double lastCabSampleR;
	double smoothCabAR;
	double smoothCabBR; //cab
	
	double lastRefL[10];
	double lastRefR[10];
	int cycle;	//undersampling
	
	enum {
		fix_freq,
		fix_reso,
		fix_a0,
		fix_a1,
		fix_a2,
		fix_b1,
		fix_b2,
		fix_sL1,
		fix_sL2,
		fix_sR1,
		fix_sR2,
		fix_total
	}; //fixed frequency biquad filter for ultrasonics, stereo
	double fixA[fix_total];
	double fixB[fix_total];
	double fixC[fix_total];
	double fixD[fix_total];
	double fixE[fix_total];
	double fixF[fix_total]; //filtering
	
	uint32_t fpdL;
	uint32_t fpdR;
	//default stuff

    float A;
    float B;
    float C;
    float D;
};