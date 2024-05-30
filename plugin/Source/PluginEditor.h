#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "melatonin_inspector/melatonin_inspector.h"

class ExciterParamBox : public gin::ParamBox
{
public:
    ExciterParamBox(const juce::String& name, ResonariumProcessor& proc_) : gin::ParamBox (name), proc(proc_)
    {
        setName("tst");


        addControl(new gin::Knob(proc.exciterParams.attack), 0, 0);
        addControl(new gin::Knob(proc.exciterParams.decay), 1, 0);
        addControl(new gin::Knob(proc.exciterParams.sustain), 2, 0);
        addControl(new gin::Knob(proc.exciterParams.release), 3, 0);
        // this->getChildren()[0]->setBounds(0, 0, 30, 30);
        setBounds(0, 40, 250, 200);
;    }

    ResonariumProcessor& proc;

    int idx;
};

class TestResonatorParamBox : public gin::ParamBox
{
public:
    TestResonatorParamBox(const juce::String& name, ResonariumProcessor& proc_) : gin::ParamBox (name), proc(proc_)
    {
        setName("testResonatorParams");
    }

    ResonariumProcessor& proc;


};

class ResonariumEditor : public gin::ProcessorEditor
{
public:
    ResonariumEditor (ResonariumProcessor&);
    ~ResonariumEditor() override;

    //==============================================================================
    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    ResonariumProcessor& proc;
    ExciterParamBox testBox { "EXCITER", proc };

    // std::unique_ptr<melatonin::Inspector> inspector;
    juce::TextButton inspectButton { "Inspect the UI" };
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ResonariumEditor)
};

