#ifndef RESONARIUMUTILITIES_H
#define RESONARIUMUTILITIES_H

#include <algorithm>

class ResonariumUtilities {
public:
    /**
     * @return a 14-bit MPE pitch bend number that bends the current MIDI note number to the target frequency (as close as possible).
     */
    static int calculateMPEPitchBendForFrequency(float targetFreq, int currentNoteNumber, float pitchBendRange)
    {
        // Convert target frequency to MIDI note number (can be fractional)
        float targetNoteNumber = 69.0f + 12.0f * std::log2(targetFreq / 440.0f);

        // Calculate semitone difference
        float semitonesDiff = targetNoteNumber - currentNoteNumber;

        // Convert to pitch bend value (assuming ±48 semitone range)
        // Scale factor is (16383/96) since the full range is 96 semitones (-48 to +48)
        // and the full pitch bend range is 0 to 16383
        int pitchBendValue = 8192 + static_cast<int>(std::round<float>(semitonesDiff * (16383.0f/(pitchBendRange * 2.0f))));

        // Clamp to valid range (0-16383)
        return std::clamp(pitchBendValue, 0, 16383);
    }


};



#endif //RESONARIUMUTILITIES_H
