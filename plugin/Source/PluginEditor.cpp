#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <sys/sysctl.h>

//==============================================================================
ResonariumEditor::ResonariumEditor(ResonariumProcessor& p)
    : gin::ProcessorEditor(p), proc(p), uiParams(p.uiParams)
{
    setSize(WINDOW_WIDTH, WINDOW_HEIGHT);
    auto voiceParams = proc.synth.params.voiceParams;

    titleBar.menuButton.setVisible(false);
    // logo = new juce::ImageComponent();
    // logo->setImage(juce::ImageCache::getFromMemory(BinaryData::resonarium_logo_png, BinaryData::resonarium_logo_pngSize));
    logoText = new juce::Label();
    logoText->setText("RESONARIUM", juce::dontSendNotification);
    logoText->setJustificationType(juce::Justification::centredLeft);
    logoText->setBounds(8, 0, 200, 40);
    logoText->setFont(logoText->getFont().withHeight(20.0f).withExtraKerningFactor(0.2f));
    logoText->setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.5f));
    addAndMakeVisible(logoText);

    scope = new gin::TriggeredScope(proc.scopeFifo);
    scope->setName ("scope");
    scope->setNumChannels (2);
    scope->setTriggerMode (gin::TriggeredScope::TriggerMode::Up);
    scope->setColour (gin::TriggeredScope::traceColourId + 0, findColour(gin::PluginLookAndFeel::accentColourId, true).withAlpha (0.7f));
    scope->setColour (gin::TriggeredScope::traceColourId + 1, findColour(gin::PluginLookAndFeel::accentColourId, true).withAlpha (0.7f));
    scope->setColour (gin::TriggeredScope::lineColourId, juce::Colours::transparentBlack);
    scope->setBounds(880, 5, 220, 30);
    addAndMakeVisible(scope);

    for(int i = 0; i < NUM_WAVEGUIDE_RESONATOR_BANKS; i++)
    {
        auto* wrb = new WaveguideResonatorBankParamBox_V2(
            "Waveguide Bank " + std::to_string(i + 1), proc, i, voiceParams.waveguideResonatorBankParams[i]);
        wrb->setBounds(EXCITER_BOX_WIDTH + 1, 40, RESONATOR_BANK_BOX_WIDTH, RESONATOR_BANK_BOX_HEIGHT);
        addAndMakeVisible(wrb);
    }

    for (int i = 0; i < NUM_IMPULSE_EXCITERS; i++)
    {
        SafePointer<ImpulseExciterParamBox> ptr = new ImpulseExciterParamBox(
            "Impulse Exciter", proc, i, voiceParams.impulseExciterParams[i]);
        impulseExciterParamBoxes.push_back(ptr);
        ptr->setBounds(0, TOP_MENU_BAR_HEIGHT, EXCITER_BOX_WIDTH, PARAM_BOX_SMALL_HEIGHT);
        addAndMakeVisible(*ptr);
    }

    for (int i = 0; i < NUM_NOISE_EXCITERS; i++)
    {
        SafePointer<NoiseExciterParamBox> ptr = new NoiseExciterParamBox(
            "Noise Exciter", proc, i, voiceParams.noiseExciterParams[i]);
        noiseExciterParamBoxes.push_back(ptr);
        addAndMakeVisible(*ptr);
        ptr->setBounds(0, TOP_MENU_BAR_HEIGHT + PARAM_BOX_SMALL_HEIGHT, EXCITER_BOX_WIDTH, PARAM_BOX_SMALL_HEIGHT);
    }

    for (int i = 0; i < NUM_IMPULSE_TRAIN_EXCITERS; i++)
    {
        SafePointer<ImpulseTrainExciterParamBox> ptr = new ImpulseTrainExciterParamBox(
            "Sequence Exciter ", proc, i, voiceParams.impulseTrainExciterParams[i]);
        impulseTrainExciterParamBoxes.push_back(ptr);
        addAndMakeVisible(*ptr);
        ptr->setBounds(0, TOP_MENU_BAR_HEIGHT + PARAM_BOX_SMALL_HEIGHT + PARAM_BOX_SMALL_HEIGHT, EXCITER_BOX_WIDTH,
                       PARAM_BOX_MEDIUM_HEIGHT);
    }

    auto* sampleExciterParamBox = new SampleExciterParamBox("Sampler", proc, voiceParams.sampleExciterParams);
    addAndMakeVisible(sampleExciterParamBox);
    sampleExciterParamBox->setBounds(
        0, TOP_MENU_BAR_HEIGHT + PARAM_BOX_SMALL_HEIGHT + PARAM_BOX_SMALL_HEIGHT + PARAM_BOX_MEDIUM_HEIGHT,
        EXCITER_BOX_WIDTH, PARAM_BOX_XSMALL_HEIGHT);
    auto* extInParamBox = new ExternalInputExciterParamBox("External In", proc, voiceParams.externalInputExciterParams);
    addAndMakeVisible(extInParamBox);
    extInParamBox->setBounds(
        0, TOP_MENU_BAR_HEIGHT + PARAM_BOX_SMALL_HEIGHT + PARAM_BOX_SMALL_HEIGHT + PARAM_BOX_MEDIUM_HEIGHT +
        PARAM_BOX_XSMALL_HEIGHT,
        EXCITER_BOX_WIDTH, PARAM_BOX_SMALL_HEIGHT);

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

    matrixBox = new MatrixBox("MATRIX", proc);
    addAndMakeVisible(matrixBox);
    matrixBox->setBounds(EXCITER_BOX_WIDTH + 1 + MODULATION_BOX_WIDTH + 1,
                         40 + RESONATOR_BANK_BOX_HEIGHT + PARAM_BOX_XSMALL_HEIGHT * 1.5f, MODULATION_BOX_WIDTH - 1,
                         PARAM_BOX_XSMALL_HEIGHT * 1.5f);

    modSrcBox = new ModSrcBox("SOURCES", proc);
    addAndMakeVisible(modSrcBox);
    modSrcBox->setBounds(EXCITER_BOX_WIDTH + 1 + MODULATION_BOX_WIDTH + 1,
                         40 + RESONATOR_BANK_BOX_HEIGHT + PARAM_BOX_XSMALL_HEIGHT * 1.5f, MODULATION_BOX_WIDTH - 1,
                         PARAM_BOX_XSMALL_HEIGHT * 1.5f);

    //Set up the scrollable effects column

    int scrollbarThickness = 4;
    juce::Rectangle <int> effectsColumn = juce::Rectangle<int>(EXCITER_BOX_WIDTH + 1 + RESONATOR_BANK_BOX_WIDTH + 1,
                                                               40,
                                                               EXCITER_BOX_WIDTH,
                                                               WINDOW_HEIGHT * 2); //some extra vertical space

    viewportContentComponent = new juce::Component();
    viewportContentComponent->setBounds(effectsColumn);

    juce::Rectangle <int> effectsColumnLocal = juce::Rectangle<int>(0, 0, EXCITER_BOX_WIDTH, WINDOW_HEIGHT * 2); //some extra vertical space

    filter1ParamBox = new SVFParamBox("Filter 1", proc, proc.synth.params.effectChainParams.filterParams[0]);
    filter1ParamBox->setBounds(effectsColumnLocal.removeFromTop(PARAM_BOX_XSMALL_HEIGHT));
    viewportContentComponent->addAndMakeVisible(filter1ParamBox);

    chorusParamBox = new ChorusParamBox("Chorus", proc, proc.synth.params.effectChainParams.chorusParams);
    chorusParamBox->setBounds(effectsColumnLocal.removeFromTop(PARAM_BOX_SMALL_HEIGHT));
    viewportContentComponent->addAndMakeVisible(chorusParamBox);

    phaserParamBox = new PhaserParamBox("Phaser", proc, proc.synth.params.effectChainParams.phaserParams);
    phaserParamBox->setBounds(effectsColumnLocal.removeFromTop(PARAM_BOX_SMALL_HEIGHT));
    viewportContentComponent->addAndMakeVisible(phaserParamBox);

    distortionParamBox = new DistortionParamBox("Distortion", proc, proc.synth.params.effectChainParams.distortionParams);
    distortionParamBox->setBounds(effectsColumnLocal.removeFromTop(PARAM_BOX_XSMALL_HEIGHT));
    viewportContentComponent->addAndMakeVisible(distortionParamBox);

    delayParamBox = new DelayParamBox("Delay", proc, proc.synth.params.effectChainParams.delayParams);
    delayParamBox->setBounds(effectsColumnLocal.removeFromTop(PARAM_BOX_SMALL_HEIGHT));
    viewportContentComponent->addAndMakeVisible(delayParamBox);

    reverbParamBox = new ReverbParamBox("Reverb", proc, proc.synth.params.effectChainParams.reverbParams);
    reverbParamBox->setBounds(effectsColumnLocal.removeFromTop(PARAM_BOX_SMALL_HEIGHT));
    viewportContentComponent->addAndMakeVisible(reverbParamBox);

    filter2ParamBox = new SVFParamBox("Filter 2", proc, proc.synth.params.effectChainParams.filterParams[1]);
    filter2ParamBox->setBounds(effectsColumnLocal.removeFromTop(PARAM_BOX_XSMALL_HEIGHT));
    viewportContentComponent->addAndMakeVisible(filter2ParamBox);

    viewport = new AnimatedScrollBarsViewport();
    viewport->setViewedComponent(viewportContentComponent);
    // viewport->setScrollBarsShown(true, false, true, false);
    viewport->setScrollBarThickness(5);
    viewport->setBounds(juce::Rectangle<int>(EXCITER_BOX_WIDTH + 1 + RESONATOR_BANK_BOX_WIDTH + 1,
                                                               40,
                                                               EXCITER_BOX_WIDTH,
                                                               WINDOW_HEIGHT));
    addAndMakeVisible(viewport);

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
            DBG("Bypassing resonators!");
        }
        else
        {
            uiParams.bypassResonators->setValue(0.0f);
            DBG("No longer bypassing resonators!");
        }
    };

#endif

#ifndef JUCE_DEBUG
     loudnessWarningBox = juce::AlertWindow::showScopedAsync (juce::MessageBoxOptions()
                                                   .withIconType (juce::MessageBoxIconType::WarningIcon)
                                                   .withTitle ("WARNING - PROTECT YOUR EARS! ")
                                                   .withMessage (
                                                       "Resonarium is an experimental digital waveguide synthesizer that is still in development. Waveguide synthesis is implemented via tightly-coupled audio feedback loops that interact with each other in potentially delightful - but unpredictable - ways.\n\nUnder certain configurations, undesirable positive feedback loops can manifest. These may produce high-frequency noise with unbounded gain that can damage your hearing or equipment. \n\nBefore continuing, please ensure that the maximum output gain of your host application and your audio device are configured at a safe level.")
                                                   .withButton ("I understand!"),
                                               nullptr);

    this->ResonariumEditor::resized();
#endif
}

ResonariumEditor::~ResonariumEditor()
{
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
    inspectButton.setBounds(200, 0, 100, 40);
    bypassResonatorsButton.setBounds(300, 0, 150, 40);
#endif
    ProcessorEditor::resized();
    for (auto* c : this->getChildren())
    {
        c->resized();
    }
}
