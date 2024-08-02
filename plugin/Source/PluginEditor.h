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

    // juce::TooltipWindow tooltipWindow;
    juce::ScopedMessageBox loudnessWarningBox;

    std::vector<SafePointer<WaveguideResonatorBankParamBox>> resonatorBankParamBoxes;
    std::vector<SafePointer<ImpulseExciterParamBox>> impulseExciterParamBoxes;
    std::vector<SafePointer<NoiseExciterParamBox>> noiseExciterParamBoxes;
    std::vector<SafePointer<ImpulseTrainExciterParamBox>> impulseTrainExciterParamBoxes;
    std::vector<SafePointer<LFOParamBox>> lfoParamBoxes;
    std::vector<SafePointer<RandomLFOParamBox>> randomLfoParamBoxes;
    std::vector<SafePointer<MSEGParamBox>> msegParamBoxes;
    std::vector<SafePointer<ADSRParamBox>> envelopeParamBoxes;
    SafePointer<MatrixBox> matrixBox;
    SafePointer<ModSrcBox> modSrcBox;

    std::unique_ptr<melatonin::Inspector> inspector;
    gin::SynthesiserUsage usage { proc.synth };

    //debug buttons, invisible on a release build
    juce::TextButton inspectButton{"INSPECT UI"};
    juce::TextButton bypassResonatorsButton{"BYPASS RESONATORS"};
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ResonariumEditor)
};
