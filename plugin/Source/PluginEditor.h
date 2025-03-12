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
    
    // Handle mouse clicks for logo and text
    void mouseDown(const juce::MouseEvent& event) override
    {
        // Check if the click is on the logo or text
        if (event.eventComponent == logo.getComponent() || 
            event.eventComponent == logoText.getComponent())
        {
            showSettingsMenu();
        }
    }

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
    
    // No need to store a reference to the settings panel
    // since we'll create it on demand in showSettingsMenu()

    std::vector<SafePointer<WaveguideResonatorBankParamBox_V2>> resonatorBankParamBoxes;

    std::vector<SafePointer<ImpulseExciterParamBox>> impulseExciterParamBoxes;
    std::vector<SafePointer<NoiseExciterParamBox>> noiseExciterParamBoxes;
    std::vector<SafePointer<SequenceExciterParamBox>> impulseTrainExciterParamBoxes;


    SafePointer<AnimatedScrollBarsViewport> excitersViewport;
    SafePointer<Component> excitersViewportContentComponent;
    SafePointer<ImpulseExciterParamBox> impulseExciterParamBox;
    SafePointer<NoiseExciterParamBox> noiseExciterParamBox;
    SafePointer<SequenceExciterParamBox> impulseTrainExciterParamBox;
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
    SafePointer<GlobalParamBox> globalParamBox;

    std::unique_ptr<melatonin::Inspector> inspector;
    gin::SynthesiserUsage usage { proc.synth };
    gin::ModulationOverview modOverview { proc.globalState.modMatrix };

    //debug buttons, invisible on a release build
    juce::TextButton inspectButton{"INSPECT UI"};
    juce::TextButton bypassResonatorsButton{"BYPASS RESONATORS"};
    juce::TextButton captureButton{"CAPTURE"};
    juce::TextButton settingsButton{"SETTINGS"};
    SettingsPanel* settingsPanel;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ResonariumEditor)
};
