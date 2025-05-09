#include "PluginProcessor.h"
#include "PluginEditor.h"

#include "util/ResonariumUtilities.h"


ResonariumEditor::ResonariumEditor(ResonariumProcessor& p)
    : gin::ProcessorEditor(p), proc(p), uiParams(p.uiParams), settingsPanel(nullptr)
{
    setSize(WINDOW_WIDTH, WINDOW_HEIGHT);
    setLookAndFeel(p.lf.get());
    auto voiceParams = proc.synth.params.voiceParams;
    titleBar.menuButton.setVisible(false);
    tooltipWindow.setMillisecondsBeforeTipAppears(500);

    auto* blur = new TitleBarDropShadow();
    blur->setBounds(0, 0, 400, 40);
    addAndMakeVisible(blur);

    logo = new ResonariumLogo();
    logo->setBounds(7, 2, 37, 37);
    logo->setInterceptsMouseClicks(true, false);
    logo->addMouseListener(this, false);

    logoText = new juce::Label();
    logoText->setText("RESONARIUM", juce::dontSendNotification);
    logoText->setJustificationType(juce::Justification::centredLeft);
    logoText->setBounds(50, 0, 210, 40);
    logoText->setFont(logoText->getFont().withHeight(26.0f).withExtraKerningFactor(0.25f));
    logoText->setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.85f));
    logoText->setInterceptsMouseClicks(true, false);
    logoText->addMouseListener(this, false);
    logoText->setTooltip("Click for settings");

    versionText = new juce::Label();
    versionText->setText(proc.getOptions().pluginVersion, juce::dontSendNotification);
    versionText->setBounds(248, 0, 100, 40);
    versionText->setFont(versionText->getFont().withStyle(juce::Font::italic).withHeight(13.0f));
    versionText->setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.2f));

    // // Create and setup the settings panel
    // settingsPanel = new SettingsPanel(proc);
    // addChildComponent(settingsPanel);

#if ! JUCE_DEBUG //we use the navbar space for debugging controls when in a debug build, so don't show the logo
    addAndMakeVisible(logo);
    addAndMakeVisible(logoText);
    addAndMakeVisible(versionText);
#endif

    addChildComponent(modOverview);
    modOverview.setBounds(350, 10, 150, 20);

    scope = new gin::TriggeredScope(proc.scopeFifo);
    scope->setName("scope");
    scope->setNumChannels(2);
    scope->setTriggerMode(gin::TriggeredScope::TriggerMode::Up);
    scope->setColour(gin::TriggeredScope::traceColourId + 0,
                     findColour(gin::PluginLookAndFeel::accentColourId, true).withAlpha(0.7f));
    scope->setColour(gin::TriggeredScope::traceColourId + 1,
                     findColour(gin::PluginLookAndFeel::accentColourId, true).withAlpha(0.7f));
    scope->setColour(gin::TriggeredScope::lineColourId, juce::Colours::transparentBlack);
    scope->setBounds(880, 5, 220, 30);
    addAndMakeVisible(scope);

    for (int i = 0; i < NUM_RESONATOR_BANKS; i++)
    {
        auto* wrb = new WaveguideResonatorBankParamBox_V2(
            "Waveguide Bank " + std::to_string(i + 1), proc, i, voiceParams.waveguideResonatorBankParams[i]);
        wrb->setBounds(EXCITER_BOX_WIDTH + 1, 40, RESONATOR_BANK_BOX_WIDTH, RESONATOR_BANK_BOX_HEIGHT);
        addAndMakeVisible(wrb);
    }

    juce::Rectangle<int> excitersColumn = juce::Rectangle<int>(0,
                                                          40,
                                                          EXCITER_BOX_WIDTH,
                                                          WINDOW_HEIGHT * 2);

    excitersViewportContentComponent = new juce::Component();
    excitersViewportContentComponent->setBounds(excitersColumn);

    juce::Rectangle<int> excitersColumnLocal = juce::Rectangle<int>(0, 0, EXCITER_BOX_WIDTH, WINDOW_HEIGHT * 2);

    impulseExciterParamBox = new ImpulseExciterParamBox("Impulse Exciter", proc, 0, voiceParams.impulseExciterParams[0]);
    impulseExciterParamBox->setBounds(excitersColumnLocal.removeFromTop(PARAM_BOX_SMALL_HEIGHT));
    excitersViewportContentComponent->addAndMakeVisible(impulseExciterParamBox);

    noiseExciterParamBox = new NoiseExciterParamBox("Noise Exciter", proc, 0, voiceParams.noiseExciterParams[0]);
    noiseExciterParamBox->setBounds(excitersColumnLocal.removeFromTop(PARAM_BOX_SMALL_HEIGHT));
    excitersViewportContentComponent->addAndMakeVisible(noiseExciterParamBox);

    impulseTrainExciterParamBox = new SequenceExciterParamBox("Sequence Exciter", proc, 0, voiceParams.impulseTrainExciterParams[0]);
    impulseTrainExciterParamBox->setBounds(excitersColumnLocal.removeFromTop(PARAM_BOX_MEDIUM_HEIGHT));
    excitersViewportContentComponent->addAndMakeVisible(impulseTrainExciterParamBox);

    sampleExciterParamBox = new SampleExciterParamBox("Sampler", proc, voiceParams.sampleExciterParams);
    sampleExciterParamBox->setBounds(excitersColumnLocal.removeFromTop(PARAM_BOX_LARGE_HEIGHT));
    excitersViewportContentComponent->addAndMakeVisible(sampleExciterParamBox);

    if(proc.globalState.samplePath.isNotEmpty())
    {
        DBG(proc.globalState.logPrefix + "Editor: Loading preset sample from " + proc.globalState.samplePath);
        juce::File file = juce::File(proc.globalState.samplePath);
        sampleExciterParamBox->sampleDropper->loadFile(file);
    } else
    {
        DBG(proc.globalState.logPrefix + "Editor: No sample path found in preset, skipping");
    }

    extInParamBox = new ExternalInputExciterParamBox("External In", proc, voiceParams.externalInputExciterParams);
    extInParamBox->setBounds(excitersColumnLocal.removeFromTop(PARAM_BOX_MEDIUM_HEIGHT));
    excitersViewportContentComponent->addAndMakeVisible(extInParamBox);

    float scrollableAreaFinalHeight = excitersColumnLocal.getY();
    excitersViewportContentComponent->setBounds(excitersViewportContentComponent->getBounds().withHeight(scrollableAreaFinalHeight + PARAM_BOX_XSMALL_HEIGHT));

    excitersViewport = new AnimatedScrollBarsViewport();
    excitersViewport->setViewedComponent(excitersViewportContentComponent);
    excitersViewport->setScrollBarThickness(5);
    excitersViewport->setBounds(juce::Rectangle<int>(  0,
                                             40,
                                             EXCITER_BOX_WIDTH,
                                             WINDOW_HEIGHT));
    addAndMakeVisible(excitersViewport);


    for (int i = 0; i < NUM_ENVELOPES; i++)
    {
        SafePointer<ADSRParamBox> ptr = new ADSRParamBox(
            "ENV " + std::to_string(i + 1), proc, proc.synth.params.adsrParams[i]);
        envelopeParamBoxes.push_back(ptr);
        addAndMakeVisible(ptr);
        ptr->setBounds(EXCITER_BOX_WIDTH + 1, 40 + RESONATOR_BANK_BOX_HEIGHT, MODULATION_BOX_WIDTH,
                       PARAM_BOX_XSMALL_HEIGHT);
    }

    for (int i = 0; i < NUM_RANDOMS; i++)
    {
        SafePointer<RandomLFOParamBox> ptr = new RandomLFOParamBox(
            "RAND " + std::to_string(i + 1), proc, i, proc.synth.params.randomLfoParams[i]);
        randomLfoParamBoxes.push_back(ptr);
        addAndMakeVisible(ptr);
        ptr->setBounds(
            EXCITER_BOX_WIDTH + 1, 40 + RESONATOR_BANK_BOX_HEIGHT + PARAM_BOX_XSMALL_HEIGHT,
            MODULATION_BOX_WIDTH,
            PARAM_BOX_XSMALL_HEIGHT);
    }

    for (int i = 0; i < NUM_LFOS; i++)
    {
        SafePointer<LFOParamBox> ptr = new LFOParamBox(
            "LFO " + std::to_string(i + 1), proc, i, voiceParams.lfoParams[i]);
        lfoParamBoxes.push_back(ptr);
        addAndMakeVisible(ptr);
        ptr->setBounds(
            EXCITER_BOX_WIDTH + 1, 40 + RESONATOR_BANK_BOX_HEIGHT + PARAM_BOX_XSMALL_HEIGHT + PARAM_BOX_XSMALL_HEIGHT,
            MODULATION_BOX_WIDTH, PARAM_BOX_XSMALL_HEIGHT);
    }

    for (int i = 0; i < NUM_MSEGS; i++)
    {
        SafePointer<MSEGParamBox> ptr = new MSEGParamBox(
            "MSEG " + std::to_string(i + 1), proc, proc.synth.params.msegParams[i]);
        msegParamBoxes.push_back(ptr);
        addAndMakeVisible(ptr);
        ptr->setBounds(EXCITER_BOX_WIDTH + 1 + MODULATION_BOX_WIDTH + 1, 40 + RESONATOR_BANK_BOX_HEIGHT,
                       MODULATION_BOX_WIDTH - 1,
                       PARAM_BOX_XSMALL_HEIGHT * 1.5f);
    }

    macroParamBox = new MacroParamBox("MACROS", proc, proc.synth.params.macroParams);
    addAndMakeVisible(macroParamBox);
    macroParamBox->setBounds(EXCITER_BOX_WIDTH + 1 + MODULATION_BOX_WIDTH + 1,
                         40 + RESONATOR_BANK_BOX_HEIGHT + PARAM_BOX_XSMALL_HEIGHT * 1.5f, MODULATION_BOX_WIDTH - 1,
                         PARAM_BOX_XSMALL_HEIGHT * 1.5f);

    matrixParamBox = new MatrixParamBox("MATRIX", proc);
    addAndMakeVisible(matrixParamBox);
    matrixParamBox->setBounds(EXCITER_BOX_WIDTH + 1 + MODULATION_BOX_WIDTH + 1,
                         40 + RESONATOR_BANK_BOX_HEIGHT + PARAM_BOX_XSMALL_HEIGHT * 1.5f, MODULATION_BOX_WIDTH - 1,
                         PARAM_BOX_XSMALL_HEIGHT * 1.5f);

    modSourceParamBox = new ModSourceParamBox("SOURCES", proc);
    addAndMakeVisible(modSourceParamBox);
    modSourceParamBox->setBounds(EXCITER_BOX_WIDTH + 1 + MODULATION_BOX_WIDTH + 1,
                         40 + RESONATOR_BANK_BOX_HEIGHT + PARAM_BOX_XSMALL_HEIGHT * 1.5f, MODULATION_BOX_WIDTH - 1,
                         PARAM_BOX_XSMALL_HEIGHT * 1.5f);

    //Set up the scrollable effects column
    juce::Rectangle<int> effectsColumn = juce::Rectangle<int>(EXCITER_BOX_WIDTH + 1 + RESONATOR_BANK_BOX_WIDTH + 1,
                                                              40,
                                                              EXCITER_BOX_WIDTH,
                                                              WINDOW_HEIGHT * 2); //some extra vertical space

    effectsViewportContentComponent = new juce::Component();
    effectsViewportContentComponent->setBounds(effectsColumn);

    juce::Rectangle<int> effectsColumnLocal = juce::Rectangle<int>(0, 0, EXCITER_BOX_WIDTH, WINDOW_HEIGHT * 2);

    filter1ParamBox = new SVFParamBox("Filter 1", proc, proc.synth.params.effectChainParams.filterParams[0]);
    filter1ParamBox->setBounds(effectsColumnLocal.removeFromTop(PARAM_BOX_XSMALL_HEIGHT));
    effectsViewportContentComponent->addAndMakeVisible(filter1ParamBox);

    chorusParamBox = new ChorusParamBox("Chorus", proc, proc.synth.params.effectChainParams.chorusParams);
    chorusParamBox->setBounds(effectsColumnLocal.removeFromTop(PARAM_BOX_SMALL_HEIGHT));
    effectsViewportContentComponent->addAndMakeVisible(chorusParamBox);

    phaserParamBox = new PhaserParamBox("Phaser", proc, proc.synth.params.effectChainParams.phaserParams);
    phaserParamBox->setBounds(effectsColumnLocal.removeFromTop(PARAM_BOX_SMALL_HEIGHT));
    effectsViewportContentComponent->addAndMakeVisible(phaserParamBox);

    distortionParamBox = new DistortionParamBox("Distortion", proc, proc.synth.params.effectChainParams.distortionParams);
    distortionParamBox->setBounds(effectsColumnLocal.removeFromTop(PARAM_BOX_SMALL_HEIGHT));
    effectsViewportContentComponent->addAndMakeVisible(distortionParamBox);

    multiAmpParamBox = new MultiAmpParamBox("Amp", proc, proc.synth.params.effectChainParams.multiAmpParams);
    multiAmpParamBox->setBounds(effectsColumnLocal.removeFromTop(PARAM_BOX_XSMALL_HEIGHT));
    effectsViewportContentComponent->addAndMakeVisible(multiAmpParamBox);

    delayParamBox = new DelayParamBox("Delay", proc, proc.synth.params.effectChainParams.delayParams);
    delayParamBox->setBounds(effectsColumnLocal.removeFromTop(PARAM_BOX_SMALL_HEIGHT));
    effectsViewportContentComponent->addAndMakeVisible(delayParamBox);

    compressorParamBox = new CompressorParamBox("Compressor", proc, proc.synth.params.effectChainParams.compressorParams);
    compressorParamBox->setBounds(effectsColumnLocal.removeFromTop(PARAM_BOX_XSMALL_HEIGHT));
    effectsViewportContentComponent->addAndMakeVisible(compressorParamBox);

    reverbParamBox = new ReverbParamBox("Reverb", proc, proc.synth.params.effectChainParams.reverbParams);
    reverbParamBox->setBounds(effectsColumnLocal.removeFromTop(PARAM_BOX_SMALL_HEIGHT));
    effectsViewportContentComponent->addAndMakeVisible(reverbParamBox);

    filter2ParamBox = new SVFParamBox("Filter 2", proc, proc.synth.params.effectChainParams.filterParams[1]);
    filter2ParamBox->setBounds(effectsColumnLocal.removeFromTop(PARAM_BOX_XSMALL_HEIGHT));
    effectsViewportContentComponent->addAndMakeVisible(filter2ParamBox);

    globalParamBox = new GlobalParamBox("Global", proc, proc.synth.params.globalParams);
    globalParamBox->setBounds(effectsColumnLocal.removeFromTop(PARAM_BOX_XSMALL_HEIGHT));
    effectsViewportContentComponent->addAndMakeVisible(globalParamBox);

    //compute a rectangle that is the size of all the components in the viewport
    scrollableAreaFinalHeight = effectsColumnLocal.getY();
    effectsViewportContentComponent->setBounds(effectsViewportContentComponent->getBounds().withHeight(scrollableAreaFinalHeight + PARAM_BOX_XSMALL_HEIGHT));


    effectsViewport = new AnimatedScrollBarsViewport();
    effectsViewport->setViewedComponent(effectsViewportContentComponent);
    // viewport->setScrollBarsShown(true, false, true, false);
    effectsViewport->setScrollBarThickness(5);
    effectsViewport->setBounds(juce::Rectangle<int>(EXCITER_BOX_WIDTH + 1 + RESONATOR_BANK_BOX_WIDTH + 1,
                                             40,
                                             EXCITER_BOX_WIDTH,
                                             WINDOW_HEIGHT));
    addAndMakeVisible(effectsViewport);

    usage.setBounds(WINDOW_WIDTH - 150, 10, 110, 20);
    addAndMakeVisible(usage);
    usage.panic.onClick = [&]
    {
        proc.synth.panic();
    };


#if JUCE_DEBUG
    addAndMakeVisible(inspectButton);
    inspectButton.onClick = [&]
    {
        if (!inspector)
        {
            inspector = std::make_unique<melatonin::Inspector>(*this);
            inspector->onClose = [this]() { inspector.reset(); };
        }

        inspector->setVisible(true);
    };
    addAndMakeVisible(bypassResonatorsButton);
    bypassResonatorsButton.setColour(juce::TextButton::textColourOnId, juce::Colours::green);
    bypassResonatorsButton.setToggleable(true);
    bypassResonatorsButton.setClickingTogglesState(true);
    bypassResonatorsButton.onClick = [&]
    {
        if (!uiParams.bypassResonators->isOn())
        {
            uiParams.bypassResonators->setValue(1.0f);
            proc.globalState.bypassResonators = true;
            DBG("Bypassing resonators!");
        }
        else
        {
            uiParams.bypassResonators->setValue(0.0f);
            proc.globalState.bypassResonators = false;
            DBG("No longer bypassing resonators!");
        }
    };
    addAndMakeVisible(captureButton);
    captureButton.onClick = [&]
    {
        DBG("Capturing UI components to " + juce::File::getCurrentWorkingDirectory().getFullPathName());
        juce::File imageDirectory = juce::File::getCurrentWorkingDirectory().getChildFile("captures");
        if (!imageDirectory.exists())
            imageDirectory.createDirectory(); // Returns true if successful
        for (juce::Component* c : this->getChildren())
        {
            if (c->getWidth() > 0 && c->getHeight() > 0 && c->getName().isNotEmpty())
            {
                ResonariumUtilities::saveComponentToImage(*c, imageDirectory.getChildFile (c->getName() + ".png"), 4.0f);
            }
        }

        for (juce::Component* c : this->effectsViewportContentComponent->getChildren())
        {
            if (c->getWidth() > 0 && c->getHeight() > 0 && c->getName().isNotEmpty())
            {
                ResonariumUtilities::saveComponentToImage(*c, imageDirectory.getChildFile (c->getName() + ".png"), 4.0f);
            }
        }

        for (juce::Component* c : this->excitersViewportContentComponent->getChildren())
        {
            if (c->getWidth() > 0 && c->getHeight() > 0)
            {
                ResonariumUtilities::saveComponentToImage(*c, imageDirectory.getChildFile (c->getName() + ".png"), 5.0f);
            }
        }

        DBG("Done capturing UI components!");
    };
    addAndMakeVisible(settingsButton);
    settingsButton.setColour(juce::TextButton::textColourOnId, juce::Colours::white);
    settingsButton.onClick = [&]
    {
        showSettingsMenu();
    };
#endif

#ifndef JUCE_DEBUG
     loudnessWarningBox = juce::AlertWindow::showScopedAsync (juce::MessageBoxOptions()
                                                   .withIconType (juce::MessageBoxIconType::WarningIcon)
                                                   .withTitle ("WARNING - PROTECT YOUR EARS! ")
                                                   .withMessage (
                                                       "Resonarium is an experimental digital waveguide synthesizer that is still in development. Waveguide synthesis is implemented via tightly-coupled audio feedback loops that interact with each other in potentially delightful - but unpredictable - ways.\n\nCertain parameter configurations may induce undesirable positive feedback loops. These often produce high-frequency noise with unbounded gain that can damage your hearing or equipment. \n\nBefore continuing, please ensure that the maximum output gain of your host application and your audio device are configured at a safe level. \n\nIf you do not do so, you may be unpredictably exposed to dangerously loud audio.\n\nRemember, you can hover over a UI component to show a pop-up tip. Have fun!")
                                                   .withButton ("I understand and have taken appropriate action!"),
                                               nullptr);

    this->ResonariumEditor::resized();
#endif

    juce::Timer::callAfterDelay(300, [this] {showOnboardingWarning();});
    DBG("Done setting up ResonariumEditor!");
}

ResonariumEditor::~ResonariumEditor()
{
    scope->stopTimer();
    
    // Clean up the settings panel if it exists
    if (settingsPanel != nullptr)
    {
        delete settingsPanel;
        settingsPanel = nullptr;
    }
    
    DBG("Deleting editor...");
}

void ResonariumEditor::paint(juce::Graphics& g)
{
    // auto area = getLocalBounds();
    // g.setFont(16.0f);
    // this->titleBar.programName.setColour(juce::Label::textColourId, juce::Colours::purple);
}

void ResonariumEditor::resized()
{
#if JUCE_DEBUG
    inspectButton.setBounds(10, 0, 100, 40);
    bypassResonatorsButton.setBounds(120, 0, 150, 40);
    captureButton.setBounds(120 + 150 + 20, 0, 100, 40);
    settingsButton.setBounds(120 + 150 + 20 + 110, 0, 100, 40);
#endif
    ProcessorEditor::resized();
    for (auto* c : this->getChildren())
    {
        c->resized();
    }
}

void ResonariumEditor::showOnboardingWarning()
{
    // juce::String msg;
    //
    // auto w = std::make_shared<gin::PluginAlertWindow> ("---- About ----", "aaa", juce::AlertWindow::NoIcon, this);
    // w->addButton ("OK", 1, juce::KeyPress (juce::KeyPress::returnKey));
    // w->setLookAndFeel (slProc.lf.get());
    //
    // w->runAsync (*this, [w] (int)
    // {
    //     w->setVisible (false);
    // });
}

void ResonariumEditor::showSettingsMenu()
{
    // Create settings panel directly
    if (settingsPanel != nullptr)
    {
        // If already exists, just show it
        settingsPanel->show();
        return;
    }
    
    // Create a new settings panel
    settingsPanel = new SettingsPanel(proc, this);
    
    // Set up the close button callback
    settingsPanel->onCloseButtonClick = [this]()
    {
        if (settingsPanel != nullptr)
        {
            settingsPanel->hide();
        }
    };
    
    // Show the panel
    settingsPanel->show();
}
