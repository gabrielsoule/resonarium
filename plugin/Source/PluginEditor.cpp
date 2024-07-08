#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
ResonariumEditor::ResonariumEditor(ResonariumProcessor& p, VoiceParams voiceParams, UIParams uiParams)
    : gin::ProcessorEditor(p), proc(p), uiParams(uiParams)
{
    setSize(WINDOW_WIDTH, WINDOW_HEIGHT);

    // for (int i = 0; i < NUM_WAVEGUIDE_RESONATOR_BANKS; i++)
    // {
    //     SafePointer<WaveguideResonatorBankParamBox> ptr = new WaveguideResonatorBankParamBox(
    //         "Resonator Bank " + std::to_string(i), i, voiceParams.waveguideResonatorBankParams[i]);
    //     resonatorBankParamBoxes.push_back(ptr);
    //     addAndMakeVisible(*ptr);
    // }
    //
    // for(int i = 0; i < NUM_MODAL_RESONATOR_BANKS; i++)
    // {
    //     addAndMakeVisible(new ModalResonatorBankParamBox(
    //         "Modal Resonator Bank " + std::to_string(i), i, voiceParams.modalResonatorBankParams[i]));
    // }

    //TODO Make this NOT HARD CODED.
    addAndMakeVisible(new ModalResonatorBankParamBox(
             "Modal Bank " + std::to_string(1), proc, 0, voiceParams.modalResonatorBankParams[0]));
    addAndMakeVisible(new WaveguideResonatorBankParamBox(
             "Waveguide Bank " + std::to_string(1), proc, 1, voiceParams.waveguideResonatorBankParams[0]));
    addAndMakeVisible(new ModalResonatorBankParamBox(
             "Modal Bank " + std::to_string(2), proc, 2, voiceParams.modalResonatorBankParams[1]));
    addAndMakeVisible(new WaveguideResonatorBankParamBox(
             "Waveguide Bank " + std::to_string(2), proc, 3, voiceParams.waveguideResonatorBankParams[1]));

    for(int i = 0; i < NUM_IMPULSE_EXCITERS; i++)
    {
        SafePointer<ImpulseExciterParamBox> ptr = new ImpulseExciterParamBox(
            "Impulse Exciter " + std::to_string(i), proc, i, voiceParams.impulseExciterParams[i]);
        impulseExciterParamBoxes.push_back(ptr);
        addAndMakeVisible(*ptr);
    }

    for(int i = 0; i < NUM_NOISE_EXCITERS; i++)
    {
        SafePointer<NoiseExciterParamBox> ptr = new NoiseExciterParamBox(
            "Noise Exciter " + std::to_string(i), proc, i, voiceParams.noiseExciterParams[i]);
        noiseExciterParamBoxes.push_back(ptr);
        addAndMakeVisible(*ptr);
    }

    for(int i = 0; i < NUM_IMPULSE_TRAIN_EXCITERS; i++)
    {
        SafePointer<ImpulseTrainExciterParamBox> ptr = new ImpulseTrainExciterParamBox(
            "Impulse Train Exciter " + std::to_string(i), proc, i, voiceParams.impulseTrainExciterParams[i]);
        impulseTrainExciterParamBoxes.push_back(ptr);
        addAndMakeVisible(*ptr);
    }





    // Melatonin Inspector -- don't modify
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
}

ResonariumEditor::~ResonariumEditor()
{

}

void ResonariumEditor::paint(juce::Graphics& g)
{
    auto area = getLocalBounds();
    g.setFont(16.0f);
    auto helloWorld = juce::String("What up!");
    g.drawText(helloWorld, area.removeFromTop(150), juce::Justification::centred, false);
    this->titleBar.programName.setColour(juce::Label::textColourId, juce::Colours::purple);
}

void ResonariumEditor::resized()
{
    auto area = getLocalBounds();
    area.removeFromBottom(50);
    inspectButton.setBounds(getLocalBounds().withSizeKeepingCentre(100, 50));
    ProcessorEditor::resized();
    for(auto* c : this->getChildren())
    {
        c->resized();
    }
}
