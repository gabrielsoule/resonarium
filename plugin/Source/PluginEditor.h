#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "melatonin_inspector/melatonin_inspector.h"
#include "ui/ResonariumComponents.h"

class ResonariumEditor : public gin::ProcessorEditor,
                         public juce::DragAndDropContainer
{
public:
    ResonariumEditor(ResonariumProcessor& p);
    ~ResonariumEditor() override;

    //==============================================================================
    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    ResonariumProcessor& proc;
    UIParams uiParams;

    std::vector<SafePointer<WaveguideResonatorBankParamBox>> resonatorBankParamBoxes;
    std::vector<SafePointer<ImpulseExciterParamBox>> impulseExciterParamBoxes;
    std::vector<SafePointer<NoiseExciterParamBox>> noiseExciterParamBoxes;
    std::vector<SafePointer<ImpulseTrainExciterParamBox>> impulseTrainExciterParamBoxes;
    std::vector<SafePointer<LFOParamBox>> lfoParamBoxes;
    std::vector<SafePointer<RandomLFOParamBox>> randomLfoParamBoxes;

    std::unique_ptr<melatonin::Inspector> inspector;
    juce::TextButton inspectButton{"Inspect the UI"};
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ResonariumEditor)
};
