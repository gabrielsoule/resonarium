#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
ResonariumEditor::ResonariumEditor (ResonariumProcessor& p)
    : gin::ProcessorEditor (p), proc (p)
{
    // addAndMakeVisible (inspectButton);

    // this chunk of code instantiates and opens the melatonin inspector
    // inspectButton.onClick = [&] {
    //     if (!inspector)
    //     {
    //         inspector = std::make_unique<melatonin::Inspector> (*this);
    //         inspector->onClose = [this]() { inspector.reset(); };
    //     }
    //
    //     inspector->setVisible (true);
    // };

    // Make sure that before the constructor has finished, you've set the
    // editor's size to whatever you need it to be.
    setSize (1200, 800);
    addAndMakeVisible(testBox);

    // addControl(new gin::Knob(proc.exciterParams.attack), 1, 0);
    // addControl(new gin::Knob(proc.exciterParams.decay), 2, 0);
    // addControl(new gin::Knob(proc.exciterParams.sustain), 3, 0);
    // addControl(new gin::Knob(proc.exciterParams.release), 4, 0);
}

ResonariumEditor::~ResonariumEditor()
{
}

void ResonariumEditor::paint (juce::Graphics& g)
{
    // g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));

    auto area = getLocalBounds();
    g.setColour (juce::Colours::white);
    g.setFont (16.0f);
    auto helloWorld = juce::String ("Hello!");
    g.drawText (helloWorld, area.removeFromTop (150), juce::Justification::centred, false);
}

//==============================================================================

void ResonariumEditor::resized()
{
    auto area = getLocalBounds();
    area.removeFromBottom(50);
    inspectButton.setBounds (getLocalBounds().withSizeKeepingCentre(100, 50));
    ProcessorEditor::resized ();
}

