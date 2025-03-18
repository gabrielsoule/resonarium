#include "ResonariumUtilities.h"
#include <algorithm>
#include <cmath>

int ResonariumUtilities::calculateMPEPitchBendForFrequency(float targetFreq,
                                                          int currentNoteNumber,
                                                          float pitchBendRange)
{
    float targetNoteNumber = 69.0f + 12.0f * std::log2(targetFreq / 440.0f);
    float semitonesDiff = targetNoteNumber - currentNoteNumber;

    // Convert to pitch bend value (assuming ±48 semitone range)
    // Scale factor is (16383/96) since the full range is 96 semitones (-48 to +48)
    // and the full pitch bend range is 0 to 16383
    int pitchBendValue = 8192 + static_cast<int>(
        round(semitonesDiff * (16383.0f / (pitchBendRange * 2.0f))));

    // Clamp to valid range (0-16383)
    return std::clamp(pitchBendValue, 0, 16383);
}

bool ResonariumUtilities::saveComponentToImage(juce::Component& comp,
                                             const juce::File& file,
                                             float scaleFactor)
{
    auto bounds = comp.getLocalBounds();
    if (auto* format = juce::ImageFileFormat::findImageFormatForFileExtension(file))
    {
        juce::FileOutputStream out(file);

        if (out.openedOk())
        {
            auto snapshot = comp.createComponentSnapshot(bounds, true, scaleFactor);
            DBG("Rendering component to " + file.getFullPathName());
            return format->writeImageToStream(snapshot, out);
        } else
        {
            DBG ("Failed to open file for writing: " + out.getStatus().getErrorMessage());
            return false;
        }
    }
    DBG("Failed to render component to " + file.getFullPathName());
    return false;
}
