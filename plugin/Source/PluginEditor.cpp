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
    mrb1->setBounds(EXCITER_BOX_WIDTH + 1, 40, WINDOW_WIDTH - 250, RESONATOR_BANK_BOX_HEIGHT);
    addAndMakeVisible(mrb1);

    auto* wrb1 = new WaveguideResonatorBankParamBox(
        "Waveguide Bank " + std::to_string(1), proc, 1, voiceParams.waveguideResonatorBankParams[0]);
    wrb1->setBounds(EXCITER_BOX_WIDTH + 1, 40, WINDOW_WIDTH - 250, RESONATOR_BANK_BOX_HEIGHT);
    addAndMakeVisible(wrb1);

    auto* mrb2 = new ModalResonatorBankParamBox(
        "Modal Bank " + std::to_string(2), proc, 2, voiceParams.modalResonatorBankParams[1]);
    mrb2->setBounds(EXCITER_BOX_WIDTH + 1, 40, WINDOW_WIDTH - 250, RESONATOR_BANK_BOX_HEIGHT);
    addAndMakeVisible(mrb2);

    auto* wrb2 = new WaveguideResonatorBankParamBox(
        "Waveguide Bank " + std::to_string(2), proc, 3, voiceParams.waveguideResonatorBankParams[1]);
    wrb2->setBounds(EXCITER_BOX_WIDTH + 1, 40, WINDOW_WIDTH - 250, RESONATOR_BANK_BOX_HEIGHT);
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

    for (int i = 0; i < NUM_LFOS; i++)
    {
        SafePointer<LFOParamBox> ptr = new LFOParamBox(
            "LFO " + std::to_string(i + 1), proc, i, voiceParams.lfoParams[i]);
        lfoParamBoxes.push_back(ptr);
        addAndMakeVisible(ptr);
        ptr->setBounds(EXCITER_BOX_WIDTH + 1, 40 + RESONATOR_BANK_BOX_HEIGHT, 400, PARAM_BOX_SMALL_HEIGHT);
    }

    for (int i = 0; i < NUM_LFOS; i++)
    {
        SafePointer<RandomLFOParamBox> ptr = new RandomLFOParamBox(
            "RAND " + std::to_string(i + 1), proc, i, proc.synth.params.randomLfoParams[i]);
        randomLfoParamBoxes.push_back(ptr);
        addAndMakeVisible(ptr);
        ptr->setBounds(EXCITER_BOX_WIDTH + 1, 40 + RESONATOR_BANK_BOX_HEIGHT + PARAM_BOX_SMALL_HEIGHT, 400,
                       PARAM_BOX_SMALL_HEIGHT);
    }
    // Melatonin Inspector -- don't modify
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
    #endif
}

ResonariumEditor::~ResonariumEditor()
{
}

void ResonariumEditor::paint(juce::Graphics& g)
{
    auto area = getLocalBounds();
    g.setFont(16.0f);
    this->titleBar.programName.setColour(juce::Label::textColourId, juce::Colours::purple);
}

void ResonariumEditor::resized()
{
#if JUCE_DEBUG
    inspectButton.setBounds(50, 0, 100, 40);
#endif
    ProcessorEditor::resized();
    for (auto* c : this->getChildren())
    {
        c->resized();
    }
}
