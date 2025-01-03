#ifndef RESONARIUMUTILITIES_H
#define RESONARIUMUTILITIES_H

#include <juce_core/juce_core.h>
#include <juce_graphics/juce_graphics.h>
#include <juce_gui_basics/juce_gui_basics.h>

class ResonariumUtilities
{
public:
    /**
     * @return a 14-bit MPE pitch bend number that bends the current MIDI note number to the target frequency (as close as possible).
     */
    static int calculateMPEPitchBendForFrequency(float targetFreq,
                                                int currentNoteNumber,
                                                float pitchBendRange);

    /**
     * Take a snapshot of the given component and save it to the specified file.
     */
    static bool saveComponentToImage(juce::Component& comp,
                                   const juce::File& file,
                                   float scaleFactor = 1.0f);

};

#endif //RESONARIUMUTILITIES_H