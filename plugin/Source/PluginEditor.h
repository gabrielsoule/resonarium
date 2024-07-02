#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "melatonin_inspector/melatonin_inspector.h"
#include "ui/ResonariumComponents.h"

class ImpulseExciterParamBox : public gin::ParamBox
{
public:
    ImpulseExciterParamBox(const juce::String& name, ResonariumProcessor& proc, int index, ImpulseExciterParams impulseParams) :
        gin::ParamBox(name), impulseParams(impulseParams)
    {
        setName("impulseExciterParams");
        setBounds(0, 40, 300, 200);
        addControl(new gin::Knob(impulseParams.thickness), 0, 0);
        addControl(new gin::Knob(impulseParams.gain), 0, 1);
        addControl(new gin::Select(impulseParams.filterParams.type), 1, 1);
        addControl(new gin::Knob(impulseParams.filterParams.frequency), 2, 1);
        addControl(new gin::Knob(impulseParams.filterParams.resonance), 3, 1);
    }

    ImpulseExciterParams impulseParams;
};

class NoiseExciterParamBox : public gin::ParamBox
{
public:
    NoiseExciterParamBox(const juce::String& name, ResonariumProcessor& proc, int index, NoiseExciterParams noiseParams) : gin::ParamBox(name)
    {
        setName("noiseExciterParams");
        setBounds(0, 40 + 200, 300, 250);
        addControl(new gin::Knob(noiseParams.type), 0, 0);
        addControl(new gin::Knob(noiseParams.gain), 1, 0);
        addControl(new gin::Knob(noiseParams.adsrParams.attack), 0, 1);
        addControl(new gin::Knob(noiseParams.adsrParams.decay), 1, 1);
        addControl(new gin::Knob(noiseParams.adsrParams.sustain), 2, 1);
        addControl(new gin::Knob(noiseParams.adsrParams.release), 3, 1);
        addControl(new gin::Knob(noiseParams.filterParams.type), 0, 2);
        addControl(new gin::Knob(noiseParams.filterParams.frequency), 1, 2);
        addControl(new gin::Knob(noiseParams.filterParams.resonance), 2, 2);
    }

    NoiseExciterParams noiseParams;
};


class WaveguideResonatorBankParamBox : public gin::ParamBox
{
public:
    WaveguideResonatorBankParamBox(const juce::String& name, ResonariumProcessor& proc, int resonatorNum,
                                   WaveguideResonatorBankParams bankParams) :
        gin::ParamBox(name), resonatorNum(resonatorNum), bankParams(bankParams), uiParams(proc.uiParams)
    {
        setName("waveguideResonatorBankParams " + juce::String(resonatorNum));
        setBounds(300 + 1, 40, WINDOW_WIDTH - 250, 450);
        addHeader({"MODAL 1", "WAVEGUIDE 1", "MODAL 2", "WAVEGUIDE 2"}, resonatorNum, uiParams.resonatorBankSelect);
        this->headerTabButtonWidth = 150;

        juce::Rectangle<int> resonatorsArea = getLocalBounds();
        resonatorsArea.removeFromTop(BOX_HEADER_HEIGHT + 10);
        resonatorsArea.setHeight(350);
        resonatorsArea.removeFromLeft(100);
        for (int i = 0; i < NUM_WAVEGUIDE_RESONATORS; i++)
        {
            resonatorsArea.removeFromLeft(7);
            WaveguideResonatorComponent* resonatorComponent = new WaveguideResonatorComponent(bankParams.resonatorParams[i]);
            resonatorComponent->setBounds(resonatorsArea.removeFromLeft(KNOB_W_SMALL));
            addAndMakeVisible(resonatorComponent);
        }

        auto* select = new gin::Select(bankParams.couplingMode);

        addControl(select);
    }

    int resonatorNum;
    WaveguideResonatorBankParams bankParams;
    UIParams uiParams;
};

class ModalResonatorBankParamBox : public gin::ParamBox
{
public:
    ModalResonatorBankParamBox(const juce::String& name, ResonariumProcessor& proc, int resonatorNum, ModalResonatorBankParams bankParams) :
        gin::ParamBox(name), resonatorNum(resonatorNum), bankParams(bankParams), uiParams(proc.uiParams)
    {
        setName("modalResonatorBankParams " + juce::String(resonatorNum));
        setBounds(300 + 1, 40, WINDOW_WIDTH - 250, 450);
        addHeader({"MODAL 1", "WAVEGUIDE 1", "MODAL 2", "WAVEGUIDE 2"}, resonatorNum, uiParams.resonatorBankSelect);
        this->headerTabButtonWidth = 150;

        juce::Rectangle<int> resonatorsArea = getLocalBounds();
        resonatorsArea.removeFromTop(BOX_HEADER_HEIGHT + 10);
        resonatorsArea.setHeight(350);
        resonatorsArea.removeFromLeft(100);

        for(int i = 0; i < NUM_MODAL_RESONATORS; i++)
        {
            resonatorsArea.removeFromLeft(7);
            ModalResonatorComponent* resonatorComponent = new ModalResonatorComponent(bankParams, i);
            resonatorComponent->setBounds(resonatorsArea.removeFromLeft(KNOB_W_SMALL));
            addAndMakeVisible(resonatorComponent);
        }
    }

    int resonatorNum;
    ModalResonatorBankParams bankParams;
    UIParams uiParams;
};

class ResonariumEditor : public gin::ProcessorEditor
{
public:
    ResonariumEditor(ResonariumProcessor&, VoiceParams params, UIParams uiParams);
    ~ResonariumEditor() override;

    //==============================================================================
    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    ResonariumProcessor& proc;
    UIParams uiParams;

    std::vector<SafePointer<WaveguideResonatorBankParamBox>> resonatorBankParamBoxes;
    std::vector<SafePointer<ImpulseExciterParamBox>> impulseExciterParamBoxes;
    std::vector<SafePointer<NoiseExciterParamBox>> noiseExciterParamBoxes;

    std::unique_ptr<melatonin::Inspector> inspector;
    juce::TextButton inspectButton{"Inspect the UI"};
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ResonariumEditor)
};
