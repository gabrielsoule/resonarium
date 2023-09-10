#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

//==============================================================================
class Editor : public gin::ProcessorEditor
{
public:
    Editor (Processor&);
    ~Editor() override;

    //==============================================================================
    void resized() override;

private:
    Processor& ginProc;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Editor)
};
