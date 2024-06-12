#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "melatonin_inspector/melatonin_inspector.h"
#include "ui/ResonariumComponents.h"

class ExciterParamBox : public gin::ParamBox
{
public:
    ExciterParamBox(const juce::String& name, ResonariumProcessor& proc_) : gin::ParamBox(name), proc(proc_)
    {
        setName("tst");

        addControl(new gin::Knob(proc.exciterParams.attack), 0, 0);
        addControl(new gin::Knob(proc.exciterParams.decay), 1, 0);
        addControl(new gin::Knob(proc.exciterParams.sustain), 2, 0);
        addControl(new gin::Knob(proc.exciterParams.release), 3, 0);
        setBounds(0, 40, 250, 200);;
        auto* testButton = new juce::ToggleButton("Enable?");
        testButton->setBounds(150, 150, 80, 40);
        addAndMakeVisible(testButton);

    }

    ResonariumProcessor& proc;

    int idx;
};


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
    ResonariumEditor(ResonariumProcessor&);
    ~ResonariumEditor() override;

    //==============================================================================
    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    ResonariumProcessor& proc;

    juce::Label myLabel;

    ExciterParamBox exciterBox{"EXCITER", proc};
    std::vector<SafePointer<ResonatorBankParamBox>> resonatorBankParamBoxes;

    std::unique_ptr<melatonin::Inspector> inspector;
    juce::TextButton inspectButton{"Inspect the UI"};
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ResonariumEditor)
};
