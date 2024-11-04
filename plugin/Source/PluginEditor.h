#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "melatonin_inspector/melatonin_inspector.h"
#include "ui/AnimatedScrollBarsViewport.h"
#include "ui/ResonariumComponents.h"
#include "ui/Panels.h"

class ResonariumEditor : public gin::ProcessorEditor,
                         public juce::DragAndDropContainer
{
public:
    ResonariumEditor(ResonariumProcessor& p);
    ~ResonariumEditor() override;

    //==============================================================================
    void paint(juce::Graphics& g) override;
    void resized() override;
    void showOnboardingWarning();

    ResonariumProcessor& proc;
    UIParams uiParams;

    // juce::TooltipWindow tooltipWindow;
    juce::ScopedMessageBox loudnessWarningBox;
    SafePointer<gin::PluginAlertWindow> loudnessWarningBox_v2;

    SafePointer<juce::ImageComponent> logoImg; //NYI TODO implement gradient logo, fow now, just text
    SafePointer<juce::Label> logoText;
    SafePointer<ResonariumLogo> logo;
    SafePointer<juce::Label> versionText;
    SafePointer<gin::TriggeredScope> scope;

    std::vector<SafePointer<WaveguideResonatorBankParamBox_V2>> resonatorBankParamBoxes;

    std::vector<SafePointer<ImpulseExciterParamBox>> impulseExciterParamBoxes;
    std::vector<SafePointer<NoiseExciterParamBox>> noiseExciterParamBoxes;
    std::vector<SafePointer<ImpulseTrainExciterParamBox>> impulseTrainExciterParamBoxes;


    SafePointer<AnimatedScrollBarsViewport> excitersViewport;
    SafePointer<Component> excitersViewportContentComponent;
    SafePointer<ImpulseExciterParamBox> impulseExciterParamBox;
    SafePointer<NoiseExciterParamBox> noiseExciterParamBox;
    SafePointer<ImpulseTrainExciterParamBox> impulseTrainExciterParamBox;
    SafePointer<SampleExciterParamBox> sampleExciterParamBox;
    SafePointer<ExternalInputExciterParamBox> extInParamBox;

    std::vector<SafePointer<LFOParamBox>> lfoParamBoxes;
    std::vector<SafePointer<RandomLFOParamBox>> randomLfoParamBoxes;
    std::vector<SafePointer<MSEGParamBox>> msegParamBoxes;
    std::vector<SafePointer<ADSRParamBox>> envelopeParamBoxes;
    SafePointer<MacroParamBox> macroParamBox;
    SafePointer<MatrixParamBox> matrixParamBox;
    SafePointer<ModSourceParamBox> modSourceParamBox;

    SafePointer<AnimatedScrollBarsViewport> effectsViewport;
    SafePointer<Component> effectsViewportContentComponent;
    SafePointer<ChorusParamBox> chorusParamBox;
    SafePointer<PhaserParamBox> phaserParamBox;
    SafePointer<DelayParamBox> delayParamBox;
    SafePointer<DistortionParamBox> distortionParamBox;
    SafePointer<MultiAmpParamBox> multiAmpParamBox;
    SafePointer<CompressorParamBox> compressorParamBox;
    SafePointer<ReverbParamBox> reverbParamBox;
    SafePointer<SVFParamBox> filter1ParamBox;
    SafePointer<SVFParamBox> filter2ParamBox;

    std::unique_ptr<melatonin::Inspector> inspector;
    gin::SynthesiserUsage usage { proc.synth };

    //debug buttons, invisible on a release build
    juce::TextButton inspectButton{"INSPECT UI"};
    juce::TextButton bypassResonatorsButton{"BYPASS RESONATORS"};
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ResonariumEditor)
};
