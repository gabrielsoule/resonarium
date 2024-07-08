#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "melatonin_inspector/melatonin_inspector.h"
#include "ui/ResonariumComponents.h"

class ImpulseExciterParamBox : public gin::ParamBox
{
public:
    ImpulseExciterParamBox(const juce::String& name, ResonariumProcessor& proc, int index,
                           ImpulseExciterParams impulseParams) :
        gin::ParamBox(name), impulseParams(impulseParams)
    {
        setName("impulseExciterParams");
        setBounds(0, 40, EXCITER_BOX_WIDTH, 165);
        addEnable(impulseParams.enabled);
        addControl(new gin::Knob(impulseParams.thickness), 0, 0);
        addControl(new gin::Knob(impulseParams.level), 0, 1);
        addControl(new gin::Select(impulseParams.filterParams.type), 1, 1);
        addControl(new gin::Knob(impulseParams.filterParams.frequency), 2, 1);
        addControl(new gin::Knob(impulseParams.filterParams.resonance), 3, 1);
    }

    ImpulseExciterParams impulseParams;
};

class NoiseExciterParamBox : public gin::ParamBox
{
public:
    NoiseExciterParamBox(const juce::String& name, ResonariumProcessor& proc, int index,
                         NoiseExciterParams noiseParams) : gin::ParamBox(name), noiseParams(noiseParams)
    {
        setName("noiseExciterParams");
        setBounds(0, 40 + 165, EXCITER_BOX_WIDTH, 165);
        addEnable(noiseParams.enabled);
        addControl(new gin::Knob(noiseParams.adsrParams.attack), 0, 0);
        addControl(new gin::Knob(noiseParams.adsrParams.decay), 1, 0);
        addControl(new gin::Knob(noiseParams.adsrParams.sustain), 2, 0);
        addControl(new gin::Knob(noiseParams.adsrParams.release), 3, 0);
        addControl(new gin::Knob(noiseParams.level), 0, 1);
        addControl(new gin::Select(noiseParams.filterParams.type), 1, 1);
        addControl(new gin::Knob(noiseParams.filterParams.frequency), 2, 1);
        addControl(new gin::Knob(noiseParams.filterParams.resonance), 3, 1);
    }

    NoiseExciterParams noiseParams;
};

class ImpulseTrainExciterParamBox : public gin::ParamBox
{
public:
    ImpulseTrainExciterParamBox(const juce::String& name, ResonariumProcessor& proc, int index,
                                ImpulseTrainExciterParams impulseTrainParams) : gin::ParamBox(name),
        impulseTrainParams(impulseTrainParams)
    {
        setName("impulseTrainExciterParams");
        setBounds(0, 40 + 165 + 165, EXCITER_BOX_WIDTH, 240);
        addEnable(impulseTrainParams.enabled);
        addControl(new gin::Select(impulseTrainParams.mode), 0, 0);
        addControl(new gin::Knob(impulseTrainParams.speed), 1, 0);
        addControl(new gin::Knob(impulseTrainParams.character), 2, 0);
        addControl(new gin::Knob(impulseTrainParams.entropy), 3, 0);
        addControl(new gin::Knob(impulseTrainParams.adsrParams.attack), 0, 1);
        addControl(new gin::Knob(impulseTrainParams.adsrParams.decay), 1, 1);
        addControl(new gin::Knob(impulseTrainParams.adsrParams.sustain), 2, 1);
        addControl(new gin::Knob(impulseTrainParams.adsrParams.release), 3, 1);
        addControl(new gin::Knob(impulseTrainParams.level), 0, 2);
        addControl(new gin::Select(impulseTrainParams.filterParams.type), 1, 2);
        addControl(new gin::Knob(impulseTrainParams.filterParams.frequency), 2, 2);
        addControl(new gin::Knob(impulseTrainParams.filterParams.resonance), 3, 2);
    }

    ImpulseTrainExciterParams impulseTrainParams;
};

class WaveguideResonatorBankParamBox : public gin::ParamBox
{
public:
    WaveguideResonatorBankParamBox(const juce::String& name, ResonariumProcessor& proc, int resonatorNum,
                                   WaveguideResonatorBankParams bankParams) :
        gin::ParamBox(name), resonatorNum(resonatorNum), bankParams(bankParams), uiParams(proc.uiParams)
    {
        setName("waveguideResonatorBankParams " + juce::String(resonatorNum));
        setBounds(EXCITER_BOX_WIDTH + 1, 40, WINDOW_WIDTH - 250, 450);
        addHeader({"MODAL 1", "WAVEGUIDE 1", "MODAL 2", "WAVEGUIDE 2"}, resonatorNum, uiParams.resonatorBankSelect);
        this->headerTabButtonWidth = 150;

        juce::Rectangle<int> resonatorsArea = getLocalBounds();
        resonatorsArea.removeFromTop(BOX_HEADER_HEIGHT + 10);
        resonatorsArea.setHeight(350);
        resonatorsArea.removeFromLeft(100);
        for (int i = 0; i < NUM_WAVEGUIDE_RESONATORS; i++)
        {
            resonatorsArea.removeFromLeft(7);
            WaveguideResonatorComponent* resonatorComponent = new WaveguideResonatorComponent(
                bankParams.resonatorParams[i]);
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
    ModalResonatorBankParamBox(const juce::String& name, ResonariumProcessor& proc, int resonatorNum,
                               ModalResonatorBankParams bankParams) :
        gin::ParamBox(name), resonatorNum(resonatorNum), bankParams(bankParams), uiParams(proc.uiParams)
    {
        setName("modalResonatorBankParams " + juce::String(resonatorNum));
        setBounds(EXCITER_BOX_WIDTH + 1, 40, WINDOW_WIDTH - 250, 450);
        addHeader({"MODAL 1", "WAVEGUIDE 1", "MODAL 2", "WAVEGUIDE 2"}, resonatorNum, uiParams.resonatorBankSelect);
        this->headerTabButtonWidth = 150;

        juce::Rectangle<int> resonatorsArea = getLocalBounds();
        resonatorsArea.removeFromTop(BOX_HEADER_HEIGHT + 10);
        resonatorsArea.setHeight(350);
        resonatorsArea.removeFromLeft(100);

        for (int i = 0; i < NUM_MODAL_RESONATORS; i++)
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
    std::vector<SafePointer<ImpulseTrainExciterParamBox>> impulseTrainExciterParamBoxes;

    std::unique_ptr<melatonin::Inspector> inspector;
    juce::TextButton inspectButton{"Inspect the UI"};
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ResonariumEditor)
};
