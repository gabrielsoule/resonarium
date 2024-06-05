//
// Created by Gabriel Soule on 6/4/24.
//

#include "Parameters.h"
#include "PluginProcessor.h"

void ExciterParams::setup(ResonariumProcessor& p)
{
    attack = p.addExtParam("exciterAttack", "Attack", "A", "s",
                           {0.0, 30.0, 0.0, 0.2f}, 0.1f,
                           0.0f);

    decay = p.addExtParam("exciterDecay", "Decay", "D", "s",
                          {0.0, 30.0, 0.0, 0.2f}, 0.1f,
                          0.0f);

    sustain = p.addExtParam("exciterSustain", "Sustain", "S", "s",
                            {0.0, 100, 0.0, 1.0}, 50.0f,
                            0.0f);
    sustain->conversionFunction = [](const float x) { return x / 100.0f; };

    release = p.addExtParam("exciterRelease", "Release", "R", "s",
                            {0.0, 30.0, 0.0, 0.2f}, 0.1f,
                            0.0f);

    level = p.addExtParam("exciterLevel", "Level", "L", "dB",
                          {0.0, 60.0, 0.0, 0.2f}, 0.1f,
                          0.0f);
}

void ResonatorBankParams::setup(ResonariumProcessor& p)
{

}
