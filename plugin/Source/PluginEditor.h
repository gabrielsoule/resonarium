#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "melatonin_inspector/melatonin_inspector.h"

//==============================================================================
class ResonariumEditor : public gin::ProcessorEditor
{
public:
    ResonariumEditor (ResonariumProcessor&);
    ~ResonariumEditor() override;

    //==============================================================================
    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    ResonariumProcessor& proc;

    // std::unique_ptr<melatonin::Inspector> inspector;
    juce::TextButton inspectButton { "Inspect the UI" };
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ResonariumEditor)
};
