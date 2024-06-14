#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "melatonin_inspector/melatonin_inspector.h"
#include "ui/ResonariumComponents.h"

class ResonatorBankParamBox : public gin::ParamBox
{
public:
    ResonatorBankParamBox(const juce::String& name, int resonatorNum_, ResonatorBankParams bankParams) :
        gin::ParamBox(name), resonatorNum(resonatorNum_), bankParams(bankParams)
    {
        setName("resonatorBankParams");
        setBounds(250 + 1, 40, WINDOW_WIDTH - 250, 450);


        juce::Rectangle<int> resonatorsArea = getLocalBounds(); //the block of screen where the resonator bank's resonators are drawn
        resonatorsArea.removeFromTop(BOX_HEADER_HEIGHT + 10);
        resonatorsArea.setHeight(350);
        for(int i = 0; i < NUM_RESONATORS; i++)
        {
            resonatorsArea.removeFromLeft(7);
            ResonatorComponent* resonatorComponent = new ResonatorComponent(bankParams.resonatorParams[i]);
            resonatorComponent->setBounds(resonatorsArea.removeFromLeft(KNOB_W_SMALL));
            addAndMakeVisible(resonatorComponent);

        }
    }

    int resonatorNum;
    ResonatorBankParams bankParams;
};

class ResonariumEditor : public gin::ProcessorEditor
{
public:
    ResonariumEditor(ResonariumProcessor&, VoiceParams params);
    ~ResonariumEditor() override;

    //==============================================================================
    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    ResonariumProcessor& proc;

    std::vector<SafePointer<ResonatorBankParamBox>> resonatorBankParamBoxes;

    std::unique_ptr<melatonin::Inspector> inspector;
    juce::TextButton inspectButton{"Inspect the UI"};
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ResonariumEditor)
};
