#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
ResonariumEditor::ResonariumEditor(ResonariumProcessor& p, VoiceParams voiceParams)
    : gin::ProcessorEditor(p), proc(p)
{
    setSize(WINDOW_WIDTH, WINDOW_HEIGHT);

    for (int i = 0; i < NUM_WAVEGUIDE_RESONATOR_BANKS; i++)
    {
        SafePointer<ResonatorBankParamBox> ptr = new ResonatorBankParamBox(
            "Resonator Bank " + std::to_string(i), i, voiceParams.resonatorBankParams[i]);
        resonatorBankParamBoxes.push_back(ptr);
        addAndMakeVisible(*ptr);
    }

    for(int i = 0; i < NUM_IMPULSE_EXCITERS; i++)
    {
        SafePointer<ImpulseExciterParamBox> ptr = new ImpulseExciterParamBox(
            "Impulse Exciter " + std::to_string(i), i, voiceParams.impulseExciterParams[i]);
        impulseExciterParamBoxes.push_back(ptr);
        addAndMakeVisible(*ptr);
    }

    for(int i = 0; i < NUM_NOISE_EXCITERS; i++)
    {
        SafePointer<NoiseExciterParamBox> ptr = new NoiseExciterParamBox(
            "Noise Exciter " + std::to_string(i), i, voiceParams.noiseExciterParams[i]);
        noiseExciterParamBoxes.push_back(ptr);
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
    for (SafePointer<ResonatorBankParamBox> ptr : resonatorBankParamBoxes)
    {
        ptr.deleteAndZero();
    }
}

void ResonariumEditor::paint(juce::Graphics& g)
{
    auto area = getLocalBounds();
    g.setColour(juce::Colours::white);
    g.setFont(16.0f);
    auto helloWorld = juce::String("Hello!");
    g.drawText(helloWorld, area.removeFromTop(150), juce::Justification::centred, false);

    this->titleBar.programName.setColour(juce::Label::textColourId, juce::Colours::purple);
}

void ResonariumEditor::resized()
{
    auto area = getLocalBounds();
    area.removeFromBottom(50);
    inspectButton.setBounds(getLocalBounds().withSizeKeepingCentre(100, 50));
    ProcessorEditor::resized();
}
