#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
ResonariumEditor::ResonariumEditor(ResonariumProcessor& p)
    : gin::ProcessorEditor(p), proc(p), uiParams(p.uiParams)
{
    setSize(WINDOW_WIDTH, WINDOW_HEIGHT);
    auto voiceParams = proc.synth.params.voiceParams;

    //TODO Make this NOT HARD CODED.
    auto* mrb1 = new ModalResonatorBankParamBox(
        "Modal Bank " + std::to_string(1), proc, 0, voiceParams.modalResonatorBankParams[0]);
    mrb1->setBounds(EXCITER_BOX_WIDTH + 1, 40, 800, RESONATOR_BANK_BOX_HEIGHT);
    addAndMakeVisible(mrb1);

    auto* wrb1 = new WaveguideResonatorBankParamBox_V2(
        "Waveguide Bank " + std::to_string(1), proc, 1, voiceParams.waveguideResonatorBankParams[0]);
    wrb1->setBounds(EXCITER_BOX_WIDTH + 1, 40, 800, RESONATOR_BANK_BOX_HEIGHT);
    addAndMakeVisible(wrb1);

    auto* mrb2 = new ModalResonatorBankParamBox(
        "Modal Bank " + std::to_string(2), proc, 2, voiceParams.modalResonatorBankParams[1]);
    mrb2->setBounds(EXCITER_BOX_WIDTH + 1, 40, 800, RESONATOR_BANK_BOX_HEIGHT);
    addAndMakeVisible(mrb2);

    auto* wrb2 = new WaveguideResonatorBankParamBox_V2(
        "Waveguide Bank " + std::to_string(2), proc, 3, voiceParams.waveguideResonatorBankParams[1]);
    wrb2->setBounds(EXCITER_BOX_WIDTH + 1, 40, 800, RESONATOR_BANK_BOX_HEIGHT);
    addAndMakeVisible(wrb2);

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
    sampleExciterParamBox->setBounds(0, TOP_MENU_BAR_HEIGHT + PARAM_BOX_SMALL_HEIGHT + PARAM_BOX_SMALL_HEIGHT + PARAM_BOX_MEDIUM_HEIGHT,
                             EXCITER_BOX_WIDTH, PARAM_BOX_XSMALL_HEIGHT);
    auto* extInParamBox = new ExternalInputExciterParamBox("External In", proc, voiceParams.externalInputExciterParams);
    addAndMakeVisible(extInParamBox);
    extInParamBox->setBounds(0, TOP_MENU_BAR_HEIGHT + PARAM_BOX_SMALL_HEIGHT + PARAM_BOX_SMALL_HEIGHT + PARAM_BOX_MEDIUM_HEIGHT + PARAM_BOX_XSMALL_HEIGHT,
                             EXCITER_BOX_WIDTH, PARAM_BOX_SMALL_HEIGHT);

    for (int i = 0; i < NUM_ENVELOPES; i++)
    {
        SafePointer<ADSRParamBox> ptr = new ADSRParamBox(
            "ENV " + std::to_string(i + 1), proc, proc.synth.params.adsrParams[i]);
        envelopeParamBoxes.push_back(ptr);
        addAndMakeVisible(ptr);
        ptr->setBounds(EXCITER_BOX_WIDTH + 1, 40 + RESONATOR_BANK_BOX_HEIGHT, 400,
                       PARAM_BOX_XSMALL_HEIGHT);
    }

    for (int i = 0; i < NUM_LFOS; i++)
    {
        SafePointer<LFOParamBox> ptr = new LFOParamBox(
            "LFO " + std::to_string(i + 1), proc, i, voiceParams.lfoParams[i]);
        lfoParamBoxes.push_back(ptr);
        addAndMakeVisible(ptr);
        ptr->setBounds(EXCITER_BOX_WIDTH + 1, 40 + RESONATOR_BANK_BOX_HEIGHT + PARAM_BOX_XSMALL_HEIGHT, 400,
                       PARAM_BOX_SMALL_HEIGHT);
    }

    for (int i = 0; i < NUM_RANDOMS; i++)
    {
        SafePointer<RandomLFOParamBox> ptr = new RandomLFOParamBox(
            "RAND " + std::to_string(i + 1), proc, i, proc.synth.params.randomLfoParams[i]);
        randomLfoParamBoxes.push_back(ptr);
        addAndMakeVisible(ptr);
        ptr->setBounds(
            EXCITER_BOX_WIDTH + 1, 40 + RESONATOR_BANK_BOX_HEIGHT + PARAM_BOX_SMALL_HEIGHT + PARAM_BOX_XSMALL_HEIGHT,
            400,
            PARAM_BOX_SMALL_HEIGHT);
    }

    for (int i = 0; i < NUM_MSEGS; i++)
    {
        SafePointer<MSEGParamBox> ptr = new MSEGParamBox(
            "MSEG " + std::to_string(i + 1), proc, proc.synth.params.msegParams[i]);
        msegParamBoxes.push_back(ptr);
        addAndMakeVisible(ptr);
        ptr->setBounds(EXCITER_BOX_WIDTH + 1 + 400 + 1, 40 + RESONATOR_BANK_BOX_HEIGHT, 400, PARAM_BOX_MEDIUM_HEIGHT);
    }

    matrixBox = new MatrixBox("MATRIX", proc);
    addAndMakeVisible(matrixBox);
    matrixBox->setBounds(EXCITER_BOX_WIDTH + 1 + 400 + 1, 40 + RESONATOR_BANK_BOX_HEIGHT + PARAM_BOX_MEDIUM_HEIGHT, 400,
                         PARAM_BOX_MEDIUM_HEIGHT);

    modSrcBox = new ModSrcBox("SOURCES", proc);
    addAndMakeVisible(modSrcBox);
    modSrcBox->setBounds(EXCITER_BOX_WIDTH + 1 + 400 + 1, 40 + RESONATOR_BANK_BOX_HEIGHT + PARAM_BOX_MEDIUM_HEIGHT, 400,
                         PARAM_BOX_MEDIUM_HEIGHT);

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
    inspectButton.setBounds(50, 0, 100, 40);
    bypassResonatorsButton.setBounds(150, 0, 150, 40);
#endif
    ProcessorEditor::resized();
    for (auto* c : this->getChildren())
    {
        c->resized();
    }
}
