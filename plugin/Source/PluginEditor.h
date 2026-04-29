#pragma once

#include "PluginProcessor.h"
#include "melatonin_inspector/melatonin_inspector.h"
#include "ui/AnimatedScrollBarsViewport.h"
#include "ui/ResonariumComponents.h"
#include "ui/Panels.h"
#include "ui/SettingsPanel.h"

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
    void showSettingsMenu();  // Method to show settings menu

    ResonariumProcessor& proc;
    UIParams uiParams;

    // juce::TooltipWindow tooltipWindow;
    juce::ScopedMessageBox loudnessWarningBox;

    std::unique_ptr<TitleBarDropShadow> titleBarDropShadow;
    std::unique_ptr<ClickableLabel> logoText;
    std::unique_ptr<ResonariumLogo> logo;
    std::unique_ptr<juce::Label> versionText;
    std::unique_ptr<gin::TriggeredScope> scope;

    std::vector<std::unique_ptr<WaveguideResonatorBankParamBox_V2>> resonatorBankParamBoxes;

    std::unique_ptr<AnimatedScrollBarsViewport> excitersViewport;
    std::unique_ptr<juce::Component> excitersViewportContentComponent;
    std::unique_ptr<ImpulseExciterParamBox> impulseExciterParamBox;
    std::unique_ptr<NoiseExciterParamBox> noiseExciterParamBox;
    std::unique_ptr<SequenceExciterParamBox> impulseTrainExciterParamBox;
    std::unique_ptr<SampleExciterParamBox> sampleExciterParamBox;
    std::unique_ptr<ExternalInputExciterParamBox> extInParamBox;

    std::vector<std::unique_ptr<LFOParamBox>> lfoParamBoxes;
    std::vector<std::unique_ptr<RandomLFOParamBox>> randomLfoParamBoxes;
    std::vector<std::unique_ptr<MSEGParamBox>> msegParamBoxes;
    std::vector<std::unique_ptr<ADSRParamBox>> envelopeParamBoxes;
    std::unique_ptr<MacroParamBox> macroParamBox;
    std::unique_ptr<MatrixParamBox> matrixParamBox;
    std::unique_ptr<ModSourceParamBox> modSourceParamBox;

    std::unique_ptr<AnimatedScrollBarsViewport> effectsViewport;
    std::unique_ptr<juce::Component> effectsViewportContentComponent;
    std::unique_ptr<ChorusParamBox> chorusParamBox;
    std::unique_ptr<PhaserParamBox> phaserParamBox;
    std::unique_ptr<DelayParamBox> delayParamBox;
    std::unique_ptr<DistortionParamBox> distortionParamBox;
    std::unique_ptr<MultiAmpParamBox> multiAmpParamBox;
    std::unique_ptr<CompressorParamBox> compressorParamBox;
    std::unique_ptr<ReverbParamBox> reverbParamBox;
    std::unique_ptr<SVFParamBox> filter1ParamBox;
    std::unique_ptr<SVFParamBox> filter2ParamBox;
    std::unique_ptr<GlobalParamBox> globalParamBox;

    std::unique_ptr<melatonin::Inspector> inspector;
    gin::SynthesiserUsage usage { proc.synth };
    gin::ModulationOverview modOverview { proc.globalState.modMatrix };

    //debug buttons, invisible on a release build
    juce::TextButton inspectButton{"INSPECT UI"};
    juce::TextButton bypassResonatorsButton{"BYPASS RESONATORS"};
    juce::TextButton captureButton{"CAPTURE"};
    juce::TextButton settingsButton{"SETTINGS"};
    std::unique_ptr<SettingsPanel> settingsPanel;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ResonariumEditor)
};
