#ifndef TOOLTIPMANAGER_H
#define TOOLTIPMANAGER_H

#include <JuceHeader.h>

class TooltipManager
{
public:
    TooltipManager();
    juce::String getTooltip(const juce::String& path);

private:
    juce::var tooltipData;
};



#endif //TOOLTIPMANAGER_H
