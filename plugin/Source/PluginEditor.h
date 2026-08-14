#pragma once

#include "PluginProcessor.h"
#include "melatonin_inspector/melatonin_inspector.h"
#include "ui/AnimatedScrollBarsViewport.h"
#include "ui/ResonariumComponents.h"
#include "ui/LazyTabbedPages.h"
#include "ui/Panels.h"
#include "ui/SettingsPanel.h"
#include "ui/UIUpdateScheduler.h"

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

private:
    void createUIUpdateScheduler();
    void configureFixedUIUpdates();
    void setVisualConsumersActive(bool active) noexcept;

    // Tab-page factories: each builds one fully positioned page and wires
    // its visualizer into the shared update scheduler.
    std::unique_ptr<juce::Component> createResonatorBankPage(int index);
    std::unique_ptr<juce::Component> createEnvelopePage(int index);
    std::unique_ptr<juce::Component> createRandomLfoPage(int index);
    std::unique_ptr<juce::Component> createLfoPage(int index);
    std::unique_ptr<juce::Component> createMsegPage(int index);
    std::unique_ptr<juce::Component> createModWindowPage(int index);

    // Deselected tab pages are usually deleted immediately; while a
    // drag-and-drop is active they are parked here instead, because the drag
    // source may live inside the page (gin's tab buttons switch tabs on
    // drag-hover) and deleting it would cancel the drag.
    void disposeTabPage(std::unique_ptr<juce::Component> page);
    void dragOperationEnded(const juce::DragAndDropTarget::SourceDetails&) override;
    std::vector<std::unique_ptr<juce::Component>> retiredTabPages;

public:

    ResonariumProcessor& proc;
    UIParams uiParams;
    ResonatorSynth::VisualSnapshot visualSnapshot;

    // juce::TooltipWindow tooltipWindow;
    juce::ScopedMessageBox loudnessWarningBox;

    std::unique_ptr<TitleBarDropShadow> titleBarDropShadow;
    std::unique_ptr<ClickableLabel> logoText;
    std::unique_ptr<ResonariumLogo> logo;
    std::unique_ptr<juce::Label> versionText;
    std::unique_ptr<gin::TriggeredScope> scope;

    // Tabbed page groups. Exactly one page per group exists at a time;
    // deselected tabs are torn down and rebuilt on demand.
    std::unique_ptr<LazyTabbedPages> resonatorBankPages;

    std::unique_ptr<AnimatedScrollBarsViewport> excitersViewport;
    std::unique_ptr<juce::Component> excitersViewportContentComponent;
    std::unique_ptr<ImpulseExciterParamBox> impulseExciterParamBox;
    std::unique_ptr<NoiseExciterParamBox> noiseExciterParamBox;
    std::unique_ptr<SequenceExciterParamBox> impulseTrainExciterParamBox;
    std::unique_ptr<SampleExciterParamBox> sampleExciterParamBox;
    std::unique_ptr<ExternalInputExciterParamBox> extInParamBox;

    std::unique_ptr<LazyTabbedPages> envelopePages;
    std::unique_ptr<LazyTabbedPages> randomLfoPages;
    std::unique_ptr<LazyTabbedPages> lfoPages;
    std::unique_ptr<LazyTabbedPages> msegPages;
    std::unique_ptr<LazyTabbedPages> modWindowPages;

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

private:
    std::unique_ptr<UIUpdateScheduler> uiUpdates;
    bool visualConsumersActive = false;

public:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ResonariumEditor)
};
