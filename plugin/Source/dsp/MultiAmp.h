#ifndef MULTIAMP_H
#define MULTIAMP_H

#include "airwindows/BassAmp.h"
#include "airwindows/BigAmp.h"
#include "airwindows/DeRez2.h"
#include "airwindows/GrindAmp.h"
#include "airwindows/FireAmp.h"
#include "airwindows/LeadAmp.h"
#include "../Parameters.h"

#include <array>

struct MultiAmpParams;
/**
* A distortion multi-effect that wraps several amp simulation algorithms,
* as well as several Airwindows amp simulators.
*/
class MultiAmp
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

    explicit MultiAmp(std::function<double()> sampleRateCallback, MultiAmpParams params);

    void reset();

    template <typename T>
    void updateParameters(T& source)
    {
        mode = static_cast<Mode>(params.mode->getProcValue());
        const std::array<float, 4> values {
            source.getValue(params.paramA),
            source.getValue(params.paramB),
            source.getValue(params.paramC),
            source.getValue(params.paramD),
        };

        const auto updateAmp = [this, &values] (auto& amp)
        {
            const auto modeIndex = static_cast<size_t>(mode);
            auto& cached = cachedParameters[modeIndex];
            for (size_t index = 0; index < values.size(); ++index)
            {
                if (!cachedParametersValid[modeIndex] || values[index] != cached[index])
                    amp.setParameter(static_cast<int>(index), values[index]);
            }
            cached = values;
            cachedParametersValid[modeIndex] = true;
        };

        switch (mode)
        {
        case BASS:
            updateAmp(bassAmp);
            break;
        case BIG:
            updateAmp(bigAmp);
            break;
        case DEREZ:
            updateAmp(deRez2);
            break;
        case FIRE:
            updateAmp(fireAmp);
            break;
        case GRIND:
            updateAmp(grindAmp);
            break;
        case LEAD:
            updateAmp(leadAmp);
            break;
        default:
            jassertfalse;
        }
    }

    static juce::String getParameterName(Mode mode, int index);

    void process(juce::dsp::ProcessContextReplacing<float> context);

    MultiAmpParams params;

    BassAmp bassAmp;
    BigAmp bigAmp;
    DeRez2 deRez2;
    FireAmp fireAmp;
    GrindAmp grindAmp;
    LeadAmp leadAmp;

    Mode mode = LEAD;

private:
    std::array<std::array<float, 4>, 6> cachedParameters {};
    std::array<bool, 6> cachedParametersValid {};
};


#endif //MULTIAMP_H
