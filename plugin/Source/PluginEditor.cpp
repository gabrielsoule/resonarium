#include "PluginProcessor.h"
#include "PluginEditor.h"

#include "util/ResonariumUtilities.h"


ResonariumEditor::ResonariumEditor(ResonariumProcessor& p)
    : gin::ProcessorEditor(p), proc(p), uiParams(p.uiParams)
{
    setSize(WINDOW_WIDTH, WINDOW_HEIGHT);
    setLookAndFeel(p.processorOptions.lookAndFeel.get());
    titleBar.menuButton.setVisible(false);
    tooltipWindow.setMillisecondsBeforeTipAppears(500);

    // The scheduler must exist before any tab page is built: page factories
    // register their visualizers with it as they run.
    createUIUpdateScheduler();

    auto voiceParams = proc.synth.params.voiceParams;

    titleBarDropShadow = std::make_unique<TitleBarDropShadow>();
    titleBarDropShadow->setBounds(0, 0, 400, 40);
    addAndMakeVisible(*titleBarDropShadow);

    logo = std::make_unique<ResonariumLogo>();
    logo->setBounds(7, 2, 37, 37);
    logo->setInterceptsMouseClicks(false, false);

    logoText = std::make_unique<juce::Label>();
    logoText->setText("RESONARIUM", juce::dontSendNotification);
    logoText->setJustificationType(juce::Justification::centredLeft);
    logoText->setBounds(50, 0, 210, 40);
    logoText->setFont(logoText->getFont().withHeight(26.0f).withExtraKerningFactor(0.25f));
    logoText->setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.85f));
    logoText->setInterceptsMouseClicks(false, false);

    versionText = std::make_unique<juce::Label>();
    versionText->setText(proc.processorOptions.pluginVersion, juce::dontSendNotification);
    versionText->setBounds(248, 0, 100, 40);
    versionText->setFont(versionText->getFont().withStyle(juce::Font::italic).withHeight(13.0f));
    versionText->setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.2f));

#if ! JUCE_DEBUG //we use the navbar space for debugging controls when in a debug build, so don't show the logo
    addAndMakeVisible(*logo);
    addAndMakeVisible(*logoText);
    addAndMakeVisible(*versionText);
#endif

    addChildComponent(modOverview);
    modOverview.setBounds(350, 10, 150, 20);

    scope = std::make_unique<gin::TriggeredScope>(proc.scopeFifo);
    scope->setName("scope");
    scope->setNumChannels(2);
    scope->setTriggerMode(gin::TriggeredScope::TriggerMode::Up);
    scope->setColour(gin::TriggeredScope::traceColourId + 0,
                     findColour(gin::PluginLookAndFeel::accentColourId, true).withAlpha(0.7f));
    scope->setColour(gin::TriggeredScope::traceColourId + 1,
                     findColour(gin::PluginLookAndFeel::accentColourId, true).withAlpha(0.7f));
    scope->setColour(gin::TriggeredScope::lineColourId, juce::Colours::transparentBlack);
    scope->setBounds(880, 5, 220, 30);
    addAndMakeVisible(*scope);

    resonatorBankPages = std::make_unique<LazyTabbedPages>(
        *this, uiParams.resonatorBankSelect,
        [this](int index) { return createResonatorBankPage(index); },
        [this](std::unique_ptr<juce::Component> page) { disposeTabPage(std::move(page)); });

    juce::Rectangle<int> excitersColumn = juce::Rectangle<int>(0,
                                                          40,
                                                          EXCITER_BOX_WIDTH,
                                                          WINDOW_HEIGHT * 2);

    excitersViewportContentComponent = std::make_unique<juce::Component>();
    excitersViewportContentComponent->setBounds(excitersColumn);

    juce::Rectangle<int> excitersColumnLocal = juce::Rectangle<int>(0, 0, EXCITER_BOX_WIDTH, WINDOW_HEIGHT * 2);

    impulseExciterParamBox = std::make_unique<ImpulseExciterParamBox>("Impulse Exciter", proc, 0, voiceParams.impulseExciterParams[0]);
    impulseExciterParamBox->setBounds(excitersColumnLocal.removeFromTop(PARAM_BOX_SMALL_HEIGHT));
    excitersViewportContentComponent->addAndMakeVisible(*impulseExciterParamBox);

    noiseExciterParamBox = std::make_unique<NoiseExciterParamBox>("Noise Exciter", proc, 0, voiceParams.noiseExciterParams[0]);
    noiseExciterParamBox->setBounds(excitersColumnLocal.removeFromTop(PARAM_BOX_SMALL_HEIGHT));
    excitersViewportContentComponent->addAndMakeVisible(*noiseExciterParamBox);

    impulseTrainExciterParamBox = std::make_unique<SequenceExciterParamBox>("Sequence Exciter", proc, 0, voiceParams.impulseTrainExciterParams[0]);
    impulseTrainExciterParamBox->setBounds(excitersColumnLocal.removeFromTop(PARAM_BOX_MEDIUM_HEIGHT));
    excitersViewportContentComponent->addAndMakeVisible(*impulseTrainExciterParamBox);

    sampleExciterParamBox = std::make_unique<SampleExciterParamBox>("Sampler", proc, voiceParams.sampleExciterParams);
    sampleExciterParamBox->setBounds(excitersColumnLocal.removeFromTop(PARAM_BOX_LARGE_HEIGHT));
    excitersViewportContentComponent->addAndMakeVisible(*sampleExciterParamBox);

    const bool instrumentLv2HasNoInput = proc.wrapperType == juce::AudioProcessor::wrapperType_LV2
                                         && proc.getMainBusNumInputChannels() == 0;
    const auto externalInputTitle = instrumentLv2HasNoInput
                                        ? "External In (unavailable in Instrument LV2)"
                                        : "External In";
    extInParamBox = std::make_unique<ExternalInputExciterParamBox>(externalInputTitle, proc, voiceParams.externalInputExciterParams);
    extInParamBox->setBounds(excitersColumnLocal.removeFromTop(PARAM_BOX_MEDIUM_HEIGHT));
    excitersViewportContentComponent->addAndMakeVisible(*extInParamBox);
    if (instrumentLv2HasNoInput)
    {
        extInParamBox->setEnabled(false);
    }

    float scrollableAreaFinalHeight = excitersColumnLocal.getY();
    excitersViewportContentComponent->setBounds(excitersViewportContentComponent->getBounds().withHeight(scrollableAreaFinalHeight + PARAM_BOX_XSMALL_HEIGHT));

    excitersViewport = std::make_unique<AnimatedScrollBarsViewport>();
    excitersViewport->setViewedComponent(excitersViewportContentComponent.get(), false);
    excitersViewport->setScrollBarThickness(5);
    excitersViewport->setBounds(juce::Rectangle<int>(  0,
                                             40,
                                             EXCITER_BOX_WIDTH,
                                             WINDOW_HEIGHT));
    addAndMakeVisible(*excitersViewport);


    auto makeDisposer = [this]
    {
        return [this](std::unique_ptr<juce::Component> page) { disposeTabPage(std::move(page)); };
    };

    envelopePages = std::make_unique<LazyTabbedPages>(
        *this, uiParams.adsrSelect,
        [this](int index) { return createEnvelopePage(index); }, makeDisposer());

    randomLfoPages = std::make_unique<LazyTabbedPages>(
        *this, uiParams.randomLfoSelect,
        [this](int index) { return createRandomLfoPage(index); }, makeDisposer());

    lfoPages = std::make_unique<LazyTabbedPages>(
        *this, uiParams.lfoSelect,
        [this](int index) { return createLfoPage(index); }, makeDisposer());

    msegPages = std::make_unique<LazyTabbedPages>(
        *this, uiParams.msegSelect,
        [this](int index) { return createMsegPage(index); }, makeDisposer());

    modWindowPages = std::make_unique<LazyTabbedPages>(
        *this, uiParams.modWindowSelect,
        [this](int index) { return createModWindowPage(index); }, makeDisposer());

    //Set up the scrollable effects column
    juce::Rectangle<int> effectsColumn = juce::Rectangle<int>(EXCITER_BOX_WIDTH + 1 + RESONATOR_BANK_BOX_WIDTH + 1,
                                                              40,
                                                              EXCITER_BOX_WIDTH,
                                                              WINDOW_HEIGHT * 2); //some extra vertical space

    effectsViewportContentComponent = std::make_unique<juce::Component>();
    effectsViewportContentComponent->setBounds(effectsColumn);

    juce::Rectangle<int> effectsColumnLocal = juce::Rectangle<int>(0, 0, EXCITER_BOX_WIDTH, WINDOW_HEIGHT * 2);

    filter1ParamBox = std::make_unique<SVFParamBox>("Filter 1", proc, proc.synth.params.effectChainParams.filterParams[0]);
    filter1ParamBox->setBounds(effectsColumnLocal.removeFromTop(PARAM_BOX_XSMALL_HEIGHT));
    effectsViewportContentComponent->addAndMakeVisible(*filter1ParamBox);

    chorusParamBox = std::make_unique<ChorusParamBox>("Chorus", proc, proc.synth.params.effectChainParams.chorusParams);
    chorusParamBox->setBounds(effectsColumnLocal.removeFromTop(PARAM_BOX_SMALL_HEIGHT));
    effectsViewportContentComponent->addAndMakeVisible(*chorusParamBox);

    phaserParamBox = std::make_unique<PhaserParamBox>("Phaser", proc, proc.synth.params.effectChainParams.phaserParams);
    phaserParamBox->setBounds(effectsColumnLocal.removeFromTop(PARAM_BOX_SMALL_HEIGHT));
    effectsViewportContentComponent->addAndMakeVisible(*phaserParamBox);

    distortionParamBox = std::make_unique<DistortionParamBox>("Distortion", proc, proc.synth.params.effectChainParams.distortionParams);
    distortionParamBox->setBounds(effectsColumnLocal.removeFromTop(PARAM_BOX_SMALL_HEIGHT));
    effectsViewportContentComponent->addAndMakeVisible(*distortionParamBox);

    multiAmpParamBox = std::make_unique<MultiAmpParamBox>("Amp", proc, proc.synth.params.effectChainParams.multiAmpParams);
    multiAmpParamBox->setBounds(effectsColumnLocal.removeFromTop(PARAM_BOX_XSMALL_HEIGHT));
    effectsViewportContentComponent->addAndMakeVisible(*multiAmpParamBox);

    delayParamBox = std::make_unique<DelayParamBox>("Delay", proc, proc.synth.params.effectChainParams.delayParams);
    delayParamBox->setBounds(effectsColumnLocal.removeFromTop(PARAM_BOX_SMALL_HEIGHT));
    effectsViewportContentComponent->addAndMakeVisible(*delayParamBox);

    compressorParamBox = std::make_unique<CompressorParamBox>("Compressor", proc, proc.synth.params.effectChainParams.compressorParams);
    compressorParamBox->setBounds(effectsColumnLocal.removeFromTop(PARAM_BOX_XSMALL_HEIGHT));
    effectsViewportContentComponent->addAndMakeVisible(*compressorParamBox);

    reverbParamBox = std::make_unique<ReverbParamBox>("Reverb", proc, proc.synth.params.effectChainParams.reverbParams);
    reverbParamBox->setBounds(effectsColumnLocal.removeFromTop(PARAM_BOX_SMALL_HEIGHT));
    effectsViewportContentComponent->addAndMakeVisible(*reverbParamBox);

    filter2ParamBox = std::make_unique<SVFParamBox>("Filter 2", proc, proc.synth.params.effectChainParams.filterParams[1]);
    filter2ParamBox->setBounds(effectsColumnLocal.removeFromTop(PARAM_BOX_XSMALL_HEIGHT));
    effectsViewportContentComponent->addAndMakeVisible(*filter2ParamBox);

    globalParamBox = std::make_unique<GlobalParamBox>("Global", proc, proc.synth.params.globalParams);
    globalParamBox->setBounds(effectsColumnLocal.removeFromTop(PARAM_BOX_XSMALL_HEIGHT));
    effectsViewportContentComponent->addAndMakeVisible(*globalParamBox);

    //compute a rectangle that is the size of all the components in the viewport
    scrollableAreaFinalHeight = effectsColumnLocal.getY();
    effectsViewportContentComponent->setBounds(effectsViewportContentComponent->getBounds().withHeight(scrollableAreaFinalHeight + PARAM_BOX_XSMALL_HEIGHT));


    effectsViewport = std::make_unique<AnimatedScrollBarsViewport>();
    effectsViewport->setViewedComponent(effectsViewportContentComponent.get(), false);
    // viewport->setScrollBarsShown(true, false, true, false);
    effectsViewport->setScrollBarThickness(5);
    effectsViewport->setBounds(juce::Rectangle<int>(EXCITER_BOX_WIDTH + 1 + RESONATOR_BANK_BOX_WIDTH + 1,
                                             40,
                                             EXCITER_BOX_WIDTH,
                                             WINDOW_HEIGHT));
    addAndMakeVisible(*effectsViewport);

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
        }
        else
        {
            uiParams.bypassResonators->setValue(0.0f);
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

    juce::Timer::callAfterDelay(300, [safeThis = juce::Component::SafePointer<ResonariumEditor>(this)]
    {
        if (safeThis != nullptr)
            safeThis->showOnboardingWarning();
    });

    configureFixedUIUpdates();
    DBG("Done setting up ResonariumEditor!");
}

ResonariumEditor::~ResonariumEditor()
{
    uiUpdates.reset();
    setVisualConsumersActive(false);

    DBG("Deleting editor...");
}

void ResonariumEditor::setVisualConsumersActive(bool active) noexcept
{
    if (active == visualConsumersActive)
        return;

    visualConsumersActive = active;
    if (active)
    {
        proc.synth.acquireVisualConsumer();
        proc.acquireScopeConsumer();
    }
    else
    {
        proc.releaseScopeConsumer();
        proc.synth.releaseVisualConsumer();
    }
}

void ResonariumEditor::createUIUpdateScheduler()
{
    uiUpdates = std::make_unique<UIUpdateScheduler>(*this);
    uiUpdates->setOwnerVisibilityCallback([this](bool visible)
    {
        setVisualConsumersActive(visible);
    });
    uiUpdates->setFrameBeginCallback([this]
    {
        static_cast<void>(proc.synth.copyVisualSnapshot(visualSnapshot));
        proc.globalState.modMatrix.publishLiveValuesFrame();
    });
}

void ResonariumEditor::configureFixedUIUpdates()
{
    usage.setInternalTimerEnabled(false);
    modOverview.setInternalTimerEnabled(false);
    titleBar.setInternalTimerEnabled(false);
    scope->setInternalTimerEnabled(false);

    uiUpdates->add(usage, UIUpdateScheduler::Rate::hz4, [this]
    {
        usage.updateFromSnapshot(visualSnapshot.getVoiceTelemetry(), visualSnapshot.getCpuUsage());
    });
    uiUpdates->add(modOverview, UIUpdateScheduler::Rate::hz60,
                   [this] { modOverview.updateFromTimer(); });
    uiUpdates->add(titleBar, UIUpdateScheduler::Rate::hz30,
                   [this] { titleBar.updateFromTimer(); });
    uiUpdates->add(*scope, UIUpdateScheduler::Rate::hz30,
                   [this] { scope->updateFromTimer(); });
    uiUpdates->add(*sampleExciterParamBox->sampleDropper, UIUpdateScheduler::Rate::hz10,
                   [this] { sampleExciterParamBox->sampleDropper->updateFromTimer(); });
}

std::unique_ptr<juce::Component> ResonariumEditor::createResonatorBankPage(int index)
{
    auto box = std::make_unique<WaveguideResonatorBankParamBox_V2>(
        "Waveguide Bank " + std::to_string(index + 1), proc, index,
        proc.synth.params.voiceParams.waveguideResonatorBankParams[index]);
    box->setBounds(EXCITER_BOX_WIDTH + 1, 40, RESONATOR_BANK_BOX_WIDTH, RESONATOR_BANK_BOX_HEIGHT);
    return box;
}

std::unique_ptr<juce::Component> ResonariumEditor::createEnvelopePage(int index)
{
    auto box = std::make_unique<ADSRParamBox>(
        "ENV " + std::to_string(index + 1), proc, proc.synth.params.adsrParams[index], visualSnapshot);
    box->setBounds(EXCITER_BOX_WIDTH + 1, 40 + RESONATOR_BANK_BOX_HEIGHT, MODULATION_BOX_WIDTH,
                   PARAM_BOX_XSMALL_HEIGHT);

    box->visualizer->setInternalTimerEnabled(false);
    uiUpdates->add(*box->visualizer, UIUpdateScheduler::Rate::hz30,
                   [visualizer = box->visualizer] { visualizer->updateFromTimer(); });
    return box;
}

std::unique_ptr<juce::Component> ResonariumEditor::createRandomLfoPage(int index)
{
    auto box = std::make_unique<RandomLFOParamBox>(
        "RAND " + std::to_string(index + 1), proc, index, proc.synth.params.randomLfoParams[index], visualSnapshot);
    box->setBounds(EXCITER_BOX_WIDTH + 1, 40 + RESONATOR_BANK_BOX_HEIGHT + PARAM_BOX_XSMALL_HEIGHT,
                   MODULATION_BOX_WIDTH, PARAM_BOX_XSMALL_HEIGHT);

    uiUpdates->add(*box->visualizer, UIUpdateScheduler::Rate::hz30,
                   [visualizer = box->visualizer] { visualizer->updateFromTimer(); });
    return box;
}

std::unique_ptr<juce::Component> ResonariumEditor::createLfoPage(int index)
{
    auto box = std::make_unique<LFOParamBox>(
        "LFO " + std::to_string(index + 1), proc, index, proc.synth.params.voiceParams.lfoParams[index],
        visualSnapshot);
    box->setBounds(EXCITER_BOX_WIDTH + 1,
                   40 + RESONATOR_BANK_BOX_HEIGHT + PARAM_BOX_XSMALL_HEIGHT + PARAM_BOX_XSMALL_HEIGHT,
                   MODULATION_BOX_WIDTH, PARAM_BOX_XSMALL_HEIGHT);

    box->visualizer->setInternalTimerEnabled(false);
    uiUpdates->add(*box->visualizer, UIUpdateScheduler::Rate::hz30,
                   [visualizer = box->visualizer] { visualizer->updateFromTimer(); });
    return box;
}

std::unique_ptr<juce::Component> ResonariumEditor::createMsegPage(int index)
{
    auto box = std::make_unique<MSEGParamBox>(
        "MSEG " + std::to_string(index + 1), proc, proc.synth.params.msegParams[index], visualSnapshot);
    box->setBounds(EXCITER_BOX_WIDTH + 1 + MODULATION_BOX_WIDTH + 1, 40 + RESONATOR_BANK_BOX_HEIGHT,
                   MODULATION_BOX_WIDTH - 1, PARAM_BOX_XSMALL_HEIGHT * 1.5f);

    box->msegComponent->setInternalTimerEnabled(false);
    uiUpdates->add(*box->msegComponent, UIUpdateScheduler::Rate::hz30,
                   [visualizer = box->msegComponent] { visualizer->updateFromTimer(); });
    return box;
}

std::unique_ptr<juce::Component> ResonariumEditor::createModWindowPage(int index)
{
    std::unique_ptr<gin::ParamBox> box;
    if (index == 0)
        box = std::make_unique<MacroParamBox>("MACROS", proc, proc.synth.params.macroParams);
    else if (index == 1)
        box = std::make_unique<ModSourceParamBox>("SOURCES", proc);
    else
        box = std::make_unique<MatrixParamBox>("MATRIX", proc);

    box->setBounds(EXCITER_BOX_WIDTH + 1 + MODULATION_BOX_WIDTH + 1,
                   40 + RESONATOR_BANK_BOX_HEIGHT + PARAM_BOX_XSMALL_HEIGHT * 1.5f,
                   MODULATION_BOX_WIDTH - 1, PARAM_BOX_XSMALL_HEIGHT * 1.5f);
    return box;
}

void ResonariumEditor::disposeTabPage(std::unique_ptr<juce::Component> page)
{
    if (page == nullptr)
        return;

    if (isDragAndDropActive())
        retiredTabPages.push_back(std::move(page));
    else
        page.reset();
}

void ResonariumEditor::dragOperationEnded(const juce::DragAndDropTarget::SourceDetails& details)
{
    juce::DragAndDropContainer::dragOperationEnded(details);

    if (retiredTabPages.empty())
        return;

    // The drag source's mouse-up is still on the call stack; free the parked
    // pages once it has fully unwound.
    juce::MessageManager::callAsync([safeThis = juce::Component::SafePointer<ResonariumEditor>(this)]
    {
        if (safeThis != nullptr)
            safeThis->retiredTabPages.clear();
    });
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
