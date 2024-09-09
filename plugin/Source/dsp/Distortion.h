#ifndef DISTORTION_H
#define DISTORTION_H
#include "../PluginProcessor.h"
#include "airwindows/BassAmp.h"
#include "airwindows/BigAmp.h"
#include "airwindows/DeRez2.h"
#include "airwindows/GrindAmp.h"
#include "airwindows/FireAmp.h"
#include "airwindows/LeadAmp.h"

/**
* A distortion multi-effect that wraps several amp simulation algorithms, such as a simple cubic nonlinearity,
* as well as several Airwindows amp simulators.
*/
class Distortion
{
public:
    enum Mode
    {
        LEAD,
        FIRE,
        GRIND,
        BIG,
        DEREZ,
        BASS
    };

    explicit Distortion(ResonariumProcessor& p, DistortionParams params);

    void reset();

    template <typename T>
    void updateParameters(T& source)
    {
        mode = static_cast<Mode>(source.getValue(params.mode));
        switch (mode)
        {
        case BASS:
            bassAmp.setParameter(0, source.getValue(params.paramA));
            bassAmp.setParameter(1, source.getValue(params.paramB));
            bassAmp.setParameter(2, source.getValue(params.paramC));
            bassAmp.setParameter(3, source.getValue(params.paramD));
            break;
        case BIG:
            bigAmp.setParameter(0, source.getValue(params.paramA));
            bigAmp.setParameter(1, source.getValue(params.paramB));
            bigAmp.setParameter(2, source.getValue(params.paramC));
            bigAmp.setParameter(3, source.getValue(params.paramD));
            break;
        case DEREZ:
            deRez2.setParameter(0, source.getValue(params.paramA));
            deRez2.setParameter(1, source.getValue(params.paramB));
            deRez2.setParameter(2, source.getValue(params.paramC));
            deRez2.setParameter(3, source.getValue(params.paramD));
            break;
        case FIRE:
            fireAmp.setParameter(0, source.getValue(params.paramA));
            fireAmp.setParameter(1, source.getValue(params.paramB));
            fireAmp.setParameter(2, source.getValue(params.paramC));
            fireAmp.setParameter(3, source.getValue(params.paramD));
            break;
        case GRIND:
            grindAmp.setParameter(0, source.getValue(params.paramA));
            grindAmp.setParameter(1, source.getValue(params.paramB));
            grindAmp.setParameter(2, source.getValue(params.paramC));
            grindAmp.setParameter(3, source.getValue(params.paramD));
            break;
        case LEAD:
            leadAmp.setParameter(0, source.getValue(params.paramA));
            leadAmp.setParameter(1, source.getValue(params.paramB));
            leadAmp.setParameter(2, source.getValue(params.paramC));
            leadAmp.setParameter(3, source.getValue(params.paramD));
            break;
        default:
            jassertfalse;
        }
    }

    static juce::String getParameterName(Mode mode, int index);

    void process(juce::dsp::ProcessContextReplacing<float> context);

    ResonariumProcessor& proc;
    DistortionParams params;

    BassAmp bassAmp;
    BigAmp bigAmp;
    DeRez2 deRez2;
    FireAmp fireAmp;
    GrindAmp grindAmp;
    LeadAmp leadAmp;

    Mode mode;
};


#endif //DISTORTION_H
