#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
ResonariumEditor::ResonariumEditor (ResonariumProcessor& p)
    : gin::ProcessorEditor (p), proc (p)
{
    setSize (1200, 800);
    addAndMakeVisible(testBox);


    for(int i = 0; i < NUM_RESONATOR_BANKS; i++)
    {
        SafePointer<ResonatorBankParamBox> ptr = new ResonatorBankParamBox("Resonator Bank " + std::to_string(i), proc, i);;
        resonatorBankParamBoxes.push_back(ptr);
        addAndMakeVisible(*ptr);
    }

    addAndMakeVisible(inspectButton);

    inspectButton.onClick = [&] {
        if (!inspector)
        {
            inspector = std::make_unique<melatonin::Inspector> (*this);
            inspector->onClose = [this]() { inspector.reset(); };
        }

        inspector->setVisible (true);
    };

    this->titleBar.programName.setLookAndFeel(nullptr);

    addAndMakeVisible(myLabel);
    myLabel.setBounds(400, 400, 100, 40);
    myLabel.setText("My Label", juce::sendNotification);
    myLabel.setColour(juce::Label::textColourId, juce::Colours::blue);
    myLabel.setFont (juce::Font (4.0f, juce::Font::bold));

}

ResonariumEditor::~ResonariumEditor()
{
    for (SafePointer<ResonatorBankParamBox> ptr : resonatorBankParamBoxes)
    {
        ptr.deleteAndZero();
    }
}

void ResonariumEditor::paint (juce::Graphics& g)
{
    // g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));

    auto area = getLocalBounds();
    g.setColour (juce::Colours::white);
    g.setFont (16.0f);
    auto helloWorld = juce::String ("Hello!");
    g.drawText (helloWorld, area.removeFromTop (150), juce::Justification::centred, false);

    this->titleBar.programName.setColour(juce::Label::textColourId, juce::Colours::purple);
}

//==============================================================================

void ResonariumEditor::resized()
{
    auto area = getLocalBounds();
    area.removeFromBottom(50);
    inspectButton.setBounds (getLocalBounds().withSizeKeepingCentre(100, 50));
    ProcessorEditor::resized ();
}

