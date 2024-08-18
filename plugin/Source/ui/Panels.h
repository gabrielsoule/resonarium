#ifndef PANELS_H
#define PANELS_H

#include <JuceHeader.h>

#include "TextSlider.h"
#include "../Parameters.h"
#include "../defines.h"
#include "ResonariumComponents.h"
#include "../ResonatorVoice.h"

class GraphicsUtils
{
public:
    static void drawBracket(const juce::Graphics& g, float x, float startY, float endY)
    {
        juce::Path bracketPath;
        bracketPath.startNewSubPath(x, startY);
        bracketPath.lineTo(x - 10, startY);
        bracketPath.lineTo(x - 10, endY);
        bracketPath.lineTo(x, endY);
        g.strokePath(bracketPath, juce::PathStrokeType(2.0f));
        // DBG(bracketPath.toString());
    }
};

class ImpulseExciterParamBox : public gin::ParamBox
{
public:
    ImpulseExciterParamBox(const juce::String& name, ResonariumProcessor& proc, int index,
                           ImpulseExciterParams impulseParams) :
        gin::ParamBox(name), impulseParams(impulseParams)
    {
        setName("impulseExciterParams");
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
        addEnable(impulseTrainParams.enabled);
        addControl(new gin::Select(impulseTrainParams.mode), 0, 0);
        addControl(new gin::Knob(impulseTrainParams.rate), 1, 0);
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

class SampleExciterParamBox : public gin::ParamBox
{
public:
    SampleExciterParamBox(const juce::String& name, ResonariumProcessor& proc, SampleExciterParams sampleParams) :
        gin::ParamBox(name), sampleParams(sampleParams)
    {
        setName("sampleExciterParams");
        addEnable(sampleParams.enabled);
        gridWidth = KNOB_W_SMALL;
        gridHeight = KNOB_H_SMALL;
        gridOffsetY += 4;
        auto gain = new gin::Knob(sampleParams.gain);
        gain->internalKnobReduction = -0;
        addControl(gain, 0, 0);
        auto mix = new gin::Knob(sampleParams.mix);
        mix->internalKnobReduction = -0;
        addControl(mix, 1, 0);
        auto start = new gin::Knob(sampleParams.start);
        start->internalKnobReduction = -0;
        addControl(start, 2, 0);
        // auto test1 = new gin::Knob(sampleParams.start);
        // test1->internalKnobReduction = -0;
        // addControl(test1, 3, 0);
        // auto test2 = new gin::Knob(sampleParams.start);
        // test2->internalKnobReduction = -0;
        // addControl(test2, 4, 0);
    }

    void paint(juce::Graphics& g) override
    {
        gin::ParamBox::paint(g);
        auto sampleArea = getLocalBounds()
                          .withTrimmedTop(TOP_MENU_BAR_HEIGHT)
                          .withTrimmedBottom(13)
                          .withTrimmedLeft(160)
                          .withTrimmedRight(10);
        constexpr float cornerSize = 5.0f;
        constexpr float dashLengths[2] = {4.0f, 8.0f};
        juce::Path path;
        path.addRoundedRectangle(sampleArea, cornerSize);
        const juce::PathStrokeType strokeType(1);
        strokeType.createDashedStroke(path, path, dashLengths, 2);
        g.setColour(juce::Colours::darkgrey);
        g.strokePath(path, strokeType);
        g.setFont(static_cast<ResonariumLookAndFeel&>(getLookAndFeel()).defaultFont);
        g.drawFittedText("DRAG SAMPLE", 165, 40, 50, 50, juce::Justification::centred, 2, 1);
    }

    SampleExciterParams sampleParams;
};

class ExternalInputExciterParamBox : public gin::ParamBox
{
public:
    ExternalInputExciterParamBox(const juce::String& name, ResonariumProcessor& proc,
                                 ExternalInputExciterParams externalInputParams) : gin::ParamBox(name),
        extInParams(externalInputParams)
    {
        setName("externalInputExciterParams");
        addEnable(extInParams.enabled);
        addControl(new gin::Knob(extInParams.gain), 0, 0);
        addControl(new gin::Knob(extInParams.mix), 1, 0);
        addControl(new gin::Select(externalInputParams.filterParams.type), 0, 1);
        addControl(new gin::Knob(externalInputParams.filterParams.frequency), 1, 1);
        addControl(new gin::Knob(externalInputParams.filterParams.resonance), 2, 1);
    }

    ExternalInputExciterParams extInParams;
};

class ModalResonatorBankParamBox : public gin::ParamBox
{
public:
    ModalResonatorBankParamBox(const juce::String& name, ResonariumProcessor& proc, int resonatorNum,
                               ModalResonatorBankParams bankParams) :
        gin::ParamBox(name), resonatorNum(resonatorNum), bankParams(bankParams), uiParams(proc.uiParams)
    {
        setName("modalResonatorBankParams " + juce::String(resonatorNum));
        this->headerTabButtonWidth = 150;
        addHeader({"MODAL 1", "WAVEGUIDE 1", "MODAL 2", "WAVEGUIDE 2"}, resonatorNum, uiParams.resonatorBankSelect);
        for (int i = 0; i < NUM_MODAL_RESONATORS; i++)
        {
            auto* resonatorComponent = new ModalResonatorComponent(bankParams, i);
            resonatorComponents.add(resonatorComponent);
            addAndMakeVisible(resonatorComponent);
        }
    }

    void resized() override
    {
        ParamBox::resized();
        juce::Rectangle<int> resonatorsArea = getLocalBounds();
        resonatorsArea.removeFromTop(BOX_HEADER_HEIGHT + 10);
        resonatorsArea.setHeight(RESONATOR_BANK_BOX_HEIGHT);
        resonatorsArea.removeFromLeft(100);
        for (int i = 0; i < NUM_MODAL_RESONATORS; i++)
        {
            resonatorsArea.removeFromLeft(7);
            resonatorComponents[i]->setBounds(resonatorsArea.removeFromLeft(KNOB_W_SMALL));
        }
    }


    void paint(juce::Graphics& g) override
    {
        ParamBox::paint(g);
        // juce::Path path;
        // path.startNewSubPath(30, 30);
        // path.lineTo(30, 70);
        // path.quadraticTo(30, 100, 60, 100);
        // path.addArrow(juce::Line<float>(100, 100, 200, 200), 3, 5, 5);
        // g.setColour(juce::Colours::blue);
        // g.strokePath(path, juce::PathStrokeType(5.0f));
    }

    int resonatorNum;
    juce::Array<ModalResonatorComponent*> resonatorComponents;
    ModalResonatorBankParams bankParams;
    UIParams uiParams;
};

class WaveguideResonatorBankParamBox_V2 : public gin::ParamBox
{
public:
    WaveguideResonatorBankParamBox_V2(const juce::String& name, ResonariumProcessor& proc, int resonatorNum,
                                      WaveguideResonatorBankParams bankParams) :
        gin::ParamBox(name), resonatorBankIndex(resonatorNum), bankParams(bankParams), uiParams(proc.uiParams)
    {
        juce::Array<juce::Colour> resonatorColors;
        resonatorColors.add(juce::Colour(0xffFFADAD));
        resonatorColors.add(juce::Colour(0xffFFD6A5));
        resonatorColors.add(juce::Colour(0xffFDFFB6));
        resonatorColors.add(juce::Colour(0xffCAFFBF));
        resonatorColors.add(juce::Colour(0xff9BF6FF));
        resonatorColors.add(juce::Colour(0xffA0C4FF));
        resonatorColors.add(juce::Colour(0xffBDB2FF));
        resonatorColors.add(juce::Colour(0xffFFC6FF));
        jassert(resonatorColors.size() == NUM_WAVEGUIDE_RESONATORS);
        for (int i = 0; i < NUM_WAVEGUIDE_RESONATORS; i++)
        {
            resonatorColors.set(i, resonatorColors[i].withSaturation(1.0).withLightness(0.7));
        }
        setName("waveguideResonatorBankParams " + juce::String(resonatorNum));
        this->headerTabButtonWidth = 150;
        addHeader({"WAVEGUIDE 1", "WAVEGUIDE 2", "WAVEGUIDE 3", "WAVEGUIDE 4"}, resonatorNum,
                  uiParams.resonatorBankSelect);
        for (int i = 0; i < NUM_WAVEGUIDE_RESONATORS; i++)
        {
            auto* resonatorComponent = new WaveguideResonatorComponent_V2(
                bankParams.resonatorParams[i], resonatorColors[i]);
            resonatorComponents.add(resonatorComponent);
            addAndMakeVisible(resonatorComponent);
        }

        textColour = juce::Colours::white.withAlpha(0.5f);

        addAndMakeVisible(loopFilterLabel = new juce::Label("loopFilterLabel", "LOOP FILTER"));
        addAndMakeVisible(postFilterLabel = new juce::Label("postFilterLabel", "POST FILTER"));
        loopFilterLabel->setJustificationType(juce::Justification::centred);
        postFilterLabel->setJustificationType(juce::Justification::centred);
        loopFilterLabel->setFont(loopFilterLabel->getFont().withHeight(17).withExtraKerningFactor(0.07f));
        postFilterLabel->setFont(postFilterLabel->getFont().withHeight(17).withExtraKerningFactor(0.07f));
        loopFilterLabel->setEditable(false);
        postFilterLabel->setEditable(false);
        loopFilterLabel->setColour(juce::Label::textColourId, textColour);
        postFilterLabel->setColour(juce::Label::textColourId, textColour);

        couplingModeKnob = new gin::Select(bankParams.couplingMode);
        addControl(couplingModeKnob);
        outputGainKnob = new gin::Knob(bankParams.outputGain);
        addControl(outputGainKnob);
        inputGainKnob = new gin::Knob(bankParams.inputGain);
        addControl(inputGainKnob);
        inputMixKnob = new gin::Knob(bankParams.inputMix);
        addControl(inputMixKnob);
        cascadeAmountKnob = new gin::Knob(bankParams.cascadeLevel);
        addControl(cascadeAmountKnob);
        cascadeFilterKnob = new gin::Knob(bankParams.cascadeFilterCutoff);
        addControl(cascadeFilterKnob);
        cascadeFilterResonanceKnob = new gin::Knob(bankParams.cascadeFilterResonance);
        addControl(cascadeFilterResonanceKnob);
        cascadeFilterModeKnob = new gin::Knob(bankParams.cascadeFilterMode);
        addControl(cascadeFilterModeKnob);


        //input gain
        //input mix

        //resonator feedback mode

        //output gain
    }

    void resized() override
    {
        ParamBox::resized();
        juce::Rectangle<int> resonatorsArea = getLocalBounds();
        resonatorsArea.removeFromTop(BOX_HEADER_HEIGHT + 10);
        resonatorsArea.setHeight(RESONATOR_BANK_BOX_HEIGHT);
        resonatorsArea.removeFromLeft(155);
        for (int i = 0; i < NUM_WAVEGUIDE_RESONATORS; i++)
        {
            resonatorComponents[i]->setBounds(resonatorsArea.removeFromLeft(62));
            resonatorsArea.removeFromLeft(4);
        }

        inputGainKnob->setBounds(720, BOX_HEADER_HEIGHT + 10, KNOB_W, KNOB_H);
        couplingModeKnob->setBounds(720, BOX_HEADER_HEIGHT + 10 + KNOB_H, KNOB_W, KNOB_H);
        outputGainKnob->setBounds(720, BOX_HEADER_HEIGHT + 10 + 2 * (KNOB_H), KNOB_W, KNOB_H);
        cascadeAmountKnob->setBounds(720, BOX_HEADER_HEIGHT + 10 + 3 * (KNOB_H), KNOB_W, KNOB_H);
        cascadeFilterKnob->setBounds(720, BOX_HEADER_HEIGHT + 10 + 4 * (KNOB_H), KNOB_W, KNOB_H);
        cascadeFilterResonanceKnob->setBounds(720, BOX_HEADER_HEIGHT + 10 + 5 * (KNOB_H), KNOB_W, KNOB_H);
        cascadeFilterModeKnob->setBounds(720, BOX_HEADER_HEIGHT + 10 + 6 * (KNOB_H), KNOB_W, KNOB_H);
        inputMixKnob->setBounds(720, BOX_HEADER_HEIGHT + 10 + 7 * KNOB_H, KNOB_W, KNOB_H);


        //it's really annoying to position rotated text components, since the affine transforms affect the coordinates...
        //we just do it "by hand" here which is... also really annoying
        loopFilterLabel->setBounds(-5, 260, 115, 30);
        loopFilterLabel->setTransform(juce::AffineTransform::rotation(-juce::MathConstants<float>::halfPi,
                                                                      loopFilterLabel->getBounds().getCentreX(),
                                                                      loopFilterLabel->getBounds().getCentreY()));
        // loopFilterLabel->setTransform(juce::AffineTransform::rotation(juce::MathConstants<float>::halfPi));
        loopFilterLabel->toFront(false);

        postFilterLabel->setBounds(-5, 375, 115, 30);
        postFilterLabel->setTransform(juce::AffineTransform::rotation(-juce::MathConstants<float>::halfPi,
                                                                      postFilterLabel->getBounds().getCentreX(),
                                                                      postFilterLabel->getBounds().getCentreY()));
        postFilterLabel->toFront(false);
    }

    void paint(juce::Graphics& g) override
    {
        ParamBox::paint(g);
        juce::Font font = static_cast<ResonariumLookAndFeel&>(getLookAndFeel()).defaultFont.withHeight(17).
            withExtraKerningFactor(0.05f);
        g.setFont(font);
        //draw some attractive background rectangles
        juce::Rectangle<float> rowBackground = juce::Rectangle<float>(157, BOX_HEADER_HEIGHT + 10 + 55 + 3, 518,
                                                                      PARAMETER_HEIGHT).expanded(0, 1);
        juce::Rectangle<int> textRect = juce::Rectangle<int>(rowBackground.getX() - 115, rowBackground.getY() + 1, 105,
                                                             PARAMETER_HEIGHT);

        g.setColour(textColour);
        g.drawFittedText("GAIN", textRect.translated(0, -35), juce::Justification::centredRight, 1);


        g.setColour(juce::Colours::black);
        juce::Rectangle resonatorPitchBlockBackground = juce::Rectangle<float>(rowBackground.getX(),
                                                                               rowBackground.getY(), 518,
                                                                               2 * PARAMETER_HEIGHT + 1 *
                                                                               SPACING_Y_SMALL).
            expanded(0, 1);
        g.fillRoundedRectangle(resonatorPitchBlockBackground, 14);
        g.setColour(textColour);
        g.drawFittedText("KEYTRACK", textRect, juce::Justification::centredRight, 1);
        textRect.translate(0, PARAMETER_HEIGHT + SPACING_Y_SMALL);
        g.drawFittedText("PITCH", textRect, juce::Justification::centredRight, 1);
        rowBackground.translate(0, PARAMETER_HEIGHT + SPACING_Y_SMALL);

        g.setColour(juce::Colours::black);
        rowBackground.translate(0, PARAMETER_HEIGHT + SPACING_Y_LARGE);
        textRect.translate(0, PARAMETER_HEIGHT + SPACING_Y_LARGE);
        g.fillRoundedRectangle(rowBackground, 14);
        g.setColour(textColour);
        g.drawFittedText("DECAY", textRect, juce::Justification::centredRight, 1);

        g.setColour(juce::Colours::black);
        rowBackground.translate(0, PARAMETER_HEIGHT + SPACING_Y_LARGE);
        textRect.translate(0, PARAMETER_HEIGHT + SPACING_Y_LARGE);
        g.fillRoundedRectangle(rowBackground, 14);
        g.setColour(textColour);
        g.drawFittedText("DISP.", textRect, juce::Justification::centredRight, 1);

        g.setColour(juce::Colours::black);
        juce::Rectangle filterBlockBackground = juce::Rectangle<float>(rowBackground.getX(),
                                                                       rowBackground.getY() + PARAMETER_HEIGHT +
                                                                       SPACING_Y_LARGE + 1, 518,
                                                                       4 * PARAMETER_HEIGHT + 3 * SPACING_Y_SMALL).
            expanded(0, 1);
        g.fillRoundedRectangle(filterBlockBackground, 14);
        textRect.translate(0, PARAMETER_HEIGHT + SPACING_Y_LARGE);
        g.setColour(textColour);
        g.drawFittedText("KEYTRACK", textRect, juce::Justification::centredRight, 1);
        textRect.translate(0, PARAMETER_HEIGHT + SPACING_Y_SMALL);
        g.drawFittedText("CUTOFF", textRect, juce::Justification::centredRight, 1);
        textRect.translate(0, PARAMETER_HEIGHT + SPACING_Y_SMALL);
        g.drawFittedText("RES.", textRect, juce::Justification::centredRight, 1);
        textRect.translate(0, PARAMETER_HEIGHT + SPACING_Y_SMALL);
        g.drawFittedText("MODE", textRect, juce::Justification::centredRight, 1);

        GraphicsUtils::drawBracket(g, filterBlockBackground.getTopLeft().x - 80,
                                   filterBlockBackground.getTopLeft().y + 2,
                                   filterBlockBackground.getBottomLeft().y - 2);

        g.setColour(juce::Colours::black);
        filterBlockBackground.translate(0, 4 * PARAMETER_HEIGHT + 3 * SPACING_Y_SMALL + SPACING_Y_LARGE);
        g.fillRoundedRectangle(filterBlockBackground, 14);
        textRect.translate(0, SPACING_Y_LARGE + PARAMETER_HEIGHT);
        g.setColour(textColour);
        g.drawFittedText("KEYTRACK", textRect, juce::Justification::centredRight, 1);
        textRect.translate(0, PARAMETER_HEIGHT + SPACING_Y_SMALL);
        g.drawFittedText("CUTOFF", textRect, juce::Justification::centredRight, 1);
        textRect.translate(0, PARAMETER_HEIGHT + SPACING_Y_SMALL);
        g.drawFittedText("RES", textRect, juce::Justification::centredRight, 1);
        textRect.translate(0, PARAMETER_HEIGHT + SPACING_Y_SMALL);
        g.drawFittedText("MODE", textRect, juce::Justification::centredRight, 1);

        GraphicsUtils::drawBracket(g, filterBlockBackground.getTopLeft().x - 80,
                                   filterBlockBackground.getTopLeft().y + 2,
                                   filterBlockBackground.getBottomLeft().y - 2);
    }

    int resonatorBankIndex;
    WaveguideResonatorBankParams bankParams;
    juce::OwnedArray<WaveguideResonatorComponent_V2> resonatorComponents;
    UIParams uiParams;

    juce::Colour textColour;
    juce::Label* loopFilterLabel;
    juce::Label* postFilterLabel;

    gin::Select* couplingModeKnob;
    gin::Knob* inputMixKnob;
    gin::Knob* inputGainKnob;
    gin::Knob* outputGainKnob;
    gin::Knob* cascadeAmountKnob;
    gin::Knob* cascadeFilterKnob;
    gin::Knob* cascadeFilterResonanceKnob;
    gin::Knob* cascadeFilterModeKnob;
};

class WaveguideResonatorBankParamBox : public gin::ParamBox
{
public:
    WaveguideResonatorBankParamBox(const juce::String& name, ResonariumProcessor& proc, int resonatorNum,
                                   WaveguideResonatorBankParams bankParams) :
        gin::ParamBox(name), resonatorNum(resonatorNum), bankParams(bankParams), uiParams(proc.uiParams)
    {
        setName("waveguideResonatorBankParams " + juce::String(resonatorNum));
        this->headerTabButtonWidth = 150;
        addHeader({"MODAL 1", "WAVEGUIDE 1", "MODAL 2", "WAVEGUIDE 2"}, resonatorNum, uiParams.resonatorBankSelect);
        auto* select = new gin::Select(bankParams.couplingMode);
        addControl(select);
        for (int i = 0; i < NUM_WAVEGUIDE_RESONATORS; i++)
        {
            WaveguideResonatorComponent* resonatorComponent = new WaveguideResonatorComponent(
                bankParams.resonatorParams[i]);
            resonatorComponents.add(resonatorComponent);
            addAndMakeVisible(resonatorComponent);
        }
    }

    void resized() override
    {
        ParamBox::resized();
        juce::Rectangle<int> resonatorsArea = getLocalBounds();
        resonatorsArea.removeFromTop(BOX_HEADER_HEIGHT + 10);
        resonatorsArea.setHeight(RESONATOR_BANK_BOX_HEIGHT);
        resonatorsArea.removeFromLeft(100);
        for (int i = 0; i < NUM_WAVEGUIDE_RESONATORS; i++)
        {
            resonatorsArea.removeFromLeft(7);
            resonatorComponents[i]->setBounds(resonatorsArea.removeFromLeft(KNOB_W_SMALL));
        }
    }

    int resonatorNum;
    juce::Array<WaveguideResonatorComponent*> resonatorComponents;
    WaveguideResonatorBankParams bankParams;
    UIParams uiParams;
};

class ADSRParamBox : public gin::ParamBox
{
public:
    ADSRParamBox(const juce::String& name, ResonariumProcessor& proc, ADSRParams adsrParams) :
        gin::ParamBox(name), proc(proc), adsrParams(adsrParams)
    {
        setName("env" + juce::String(adsrParams.index + 1));
        addEnable(adsrParams.enabled);
        juce::StringArray adsrNames;
        for (int i = 0; i < NUM_ENVELOPES; i++)
        {
            adsrNames.add("ENV " + juce::String(i + 1));
        }
        addHeader(adsrNames, adsrParams.index, proc.uiParams.adsrSelect);
        headerTabButtonWidth = 75;

        addModSource(new gin::ModulationSourceButton(proc.modMatrix, proc.modSrcPolyENV[adsrParams.index], true));

        addControl(new gin::Knob(adsrParams.attack), 0, 0);
        addControl(new gin::Knob(adsrParams.decay), 1, 0);
        addControl(new gin::Knob(adsrParams.sustain), 2, 0);
        addControl(new gin::Knob(adsrParams.release), 3, 0);

        visualizer = new gin::ADSRComponent();
        visualizer->setParams(adsrParams.attack, adsrParams.decay, adsrParams.sustain, adsrParams.release);
        visualizer->phaseCallback = [this, adsrParams]
        {
            std::vector<std::pair<int, float>> res;

            if (adsrParams.enabled->isOn())
            {
                for (auto v : this->proc.synth.getActiveVoices())
                    if (auto rv = dynamic_cast<ResonatorVoice*>(v))
                        res.push_back(rv->polyEnvelopes[adsrParams.index].internalADSR.getCurrentPhase());
            }

            return res;
        };
        // addControl(g, 4, 0, 4, 1);
        addControl(visualizer);
    }

    ResonariumProcessor& proc;
    gin::ADSRComponent* visualizer;
    ADSRParams adsrParams;

    void resized() override
    {
        ParamBox::resized();
        visualizer->setBounds(
            juce::Rectangle<int>(KNOB_W * 4 + 6, TITLE_BAR_HEIGHT, KNOB_W * 4 - 6, KNOB_H).translated(2, 6));
    }
};

class LFOParamBox : public gin::ParamBox
{
public:
    LFOParamBox(const juce::String& name, ResonariumProcessor& proc, int resonatorNum, LFOParams lfoParams) :
        gin::ParamBox(name), proc(proc), lfoParams(lfoParams)
    {
        jassert(lfoParams.index != -1);
        setName("lfo" + juce::String(lfoParams.index + 1));
        addEnable(lfoParams.enabled);
        juce::StringArray lfoNames;
        for (int i = 0; i < NUM_LFOS; i++)
        {
            lfoNames.add("LFO " + juce::String(i + 1));
        }
        addHeader(lfoNames, lfoParams.index, proc.uiParams.lfoSelect);
        headerTabButtonWidth = 75;

        // addModSource (new gin::ModulationSourceButton (proc.modMatrix, proc.modSrcLfo[idx], true));
        addModSource(new gin::ModulationSourceButton(proc.modMatrix, proc.modSrcMonoLFO[lfoParams.index], false));
        addModSource(new gin::ModulationSourceButton(proc.modMatrix, proc.modSrcPolyLFO[lfoParams.index], true));

        addControl(r = new gin::Knob(lfoParams.rate), 1, 0);
        addControl(b = new gin::Select(lfoParams.beat), 1, 0);
        addControl(new gin::Knob(lfoParams.stereo), 2, 0);
        addControl(new gin::Knob(lfoParams.depth, true), 3, 0);
        // addControl(new gin::Knob(lfoParams.fade, true), 6, 0);
        // addControl(new gin::Knob(lfoParams.delay), 7, 0);

        addControl(new gin::Select(lfoParams.wave), 5, 0);
        addControl(new gin::Switch(lfoParams.sync), 0, 0);
        addControl(new gin::Knob(lfoParams.phase, true), 4, 0);
        // addControl(new gin::Knob(lfoParams.offset, true), 3, 1);

        visualizer = new gin::LFOComponent();
        visualizer->setParams(lfoParams.wave, lfoParams.sync, lfoParams.rate, lfoParams.beat, lfoParams.depth,
                              lfoParams.offset,
                              lfoParams.phase, lfoParams.enabled, lfoParams.stereo);
        jassert(lfoParams.enabled != nullptr);
        visualizer->phaseCallback = [this, lfoParams]
        {
            std::vector<float> res;

            if (lfoParams.enabled->isOn())
            {
                res.push_back(this->proc.synth.monoLFOs[lfoParams.index].getCurrentPhase(0));

                for (auto v : this->proc.synth.getActiveVoices())
                    if (auto voice = dynamic_cast<ResonatorVoice*>(v))
                        res.push_back(voice->polyLFOs[lfoParams.index].getCurrentPhase(0));
            }

            return res;
        };
        addControl(visualizer);
        watchParam(lfoParams.sync);
    }

    ResonariumProcessor& proc;
    gin::LFOComponent* visualizer;
    LFOParams lfoParams;
    gin::ParamComponent::Ptr r = nullptr;
    gin::ParamComponent::Ptr b = nullptr;

    void resized() override
    {
        ParamBox::resized();
        visualizer->setBounds(
            juce::Rectangle<int>(KNOB_W * 6 + 6, TITLE_BAR_HEIGHT, KNOB_W * 2 - 6, KNOB_H).translated(2, 6));
    }

    void paramChanged() override
    {
        gin::ParamBox::paramChanged();

        if (r && b)
        {
            r->setVisible(!lfoParams.sync->isOn());
            b->setVisible(lfoParams.sync->isOn());
        }
    }
};

class RandomLFOParamBox : public gin::ParamBox
{
public:
    RandomLFOParamBox(const juce::String& name, ResonariumProcessor& proc, int resonatorNum,
                      RandomLFOParams randomLfoParams) :
        gin::ParamBox(name), randomLfoParams(randomLfoParams)
    {
        setName("rnd" + juce::String(randomLfoParams.index + 1));
        addEnable(randomLfoParams.enabled);
        juce::StringArray lfoNames;
        for (int i = 0; i < NUM_LFOS; i++)
        {
            lfoNames.add("RAND " + juce::String(i + 1));
        }
        addHeader(lfoNames, randomLfoParams.index, proc.uiParams.randomLfoSelect);
        headerTabButtonWidth = 75;

        addModSource(new gin::ModulationSourceButton(proc.modMatrix, proc.modSrcMonoRND[randomLfoParams.index], false));
        addModSource(new gin::ModulationSourceButton(proc.modMatrix, proc.modSrcPolyRND[randomLfoParams.index], true));

        addControl(new gin::Switch(randomLfoParams.sync), 0, 0);
        addControl(r = new gin::Knob(randomLfoParams.rate), 1, 0);
        addControl(b = new gin::Select(randomLfoParams.beat), 1, 0);
        addControl(new gin::Knob(randomLfoParams.stereo), 2, 0);
        addControl(new gin::Knob(randomLfoParams.depth, true), 3, 0);
        // addControl(new gin::Knob(randomLfoParams.jitter), 3, 0);
        addControl(new gin::Knob(randomLfoParams.chaos), 4, 0);
        addControl(new gin::Knob(randomLfoParams.smooth), 5, 0);
        // addControl(new gin::Knob(randomLfoParams.offset), 3, 1);
        watchParam(randomLfoParams.sync);
    }

    void resized() override
    {
        ParamBox::resized();
    }

    void paramChanged() override
    {
        gin::ParamBox::paramChanged();

        if (r && b)
        {
            r->setVisible(!randomLfoParams.sync->isOn());
            b->setVisible(randomLfoParams.sync->isOn());
        }
    }

    RandomLFOParams randomLfoParams;
    gin::ParamComponent::Ptr r = nullptr;
    gin::ParamComponent::Ptr b = nullptr;
};

class MSEGParamBox : public gin::ParamBox
{
public:
    MSEGParamBox(const juce::String& name, ResonariumProcessor& proc, MSEGParams msegParams) :
        gin::ParamBox(name), proc(proc), msegParams(msegParams)
    {
        setName("mseg" + juce::String(msegParams.index + 1));

        addEnable(msegParams.enabled);
        juce::StringArray lfoNames;
        for (int i = 0; i < NUM_LFOS; i++)
        {
            lfoNames.add("MSEG " + juce::String(i + 1));
        }
        addHeader(lfoNames, msegParams.index, proc.uiParams.msegSelect);
        headerTabButtonWidth = 75;

        addModSource(new gin::ModulationSourceButton(proc.modMatrix, proc.modSrcPolyMSEG[msegParams.index], true));
        addModSource(new gin::ModulationSourceButton(proc.modMatrix, proc.modSrcMonoMSEG[msegParams.index], false));
        gridWidth = KNOB_W_SMALL;
        gridHeight = KNOB_H_SMALL;
        addControl(new gin::Switch(msegParams.sync), 8, 0);
        addControl(r = new gin::Knob(msegParams.rate), 9, 0);
        addControl(b = new gin::Select(msegParams.beat), 9, 0);
        addControl(new gin::Knob(msegParams.xgrid), 8, 1);
        addControl(new gin::Knob(msegParams.ygrid, true), 9, 1);


        msegComponent = new gin::MSEGComponent(*proc.synth.monoMSEGs.getReference(msegParams.index).dataPointer);
        msegComponent->setParams(msegParams.sync, msegParams.rate, msegParams.beat, msegParams.depth, msegParams.offset,
                                 msegParams.phase, msegParams.enabled, msegParams.xgrid, msegParams.ygrid,
                                 msegParams.loop);
        msegComponent->phaseCallback = [this, msegParams]
        {
            std::vector<float> res;

            if (msegParams.enabled->isOn())
            {
                res.push_back(this->proc.synth.monoMSEGs.getReference(msegParams.index).getCurrentPhase(0));

                for (auto v : this->proc.synth.getActiveVoices())
                    if (auto voice = dynamic_cast<ResonatorVoice*>(v))
                        res.push_back(voice->polyMSEGs.getReference(msegParams.index).getCurrentPhase(0));
            }

            return res;
        };
        addControl(msegComponent);
        msegComponent->setEditable(true);
        watchParam(msegParams.sync);
    }

    void resized() override
    {
        ParamBox::resized();
        auto bounds = getLocalBounds()
                      .reduced(2, 0)
                      .withTrimmedTop(BOX_HEADER_HEIGHT + 2)
                      .withTrimmedBottom(2)
                      .withTrimmedRight(82);
        msegComponent->setBounds(bounds);
    }

    void paint(juce::Graphics& g) override
    {
        gin::ParamBox::paint(g);
        // g.setColour(juce::Colours::black);
        // g.fillRoundedRectangle(msegComponent->getBoundsInParent().toFloat(), 5);
    }

    void paramChanged() override
    {
        gin::ParamBox::paramChanged();
        if (r && b)
        {
            r->setVisible(!msegParams.sync->isOn());
            b->setVisible(msegParams.sync->isOn());
        }
    }

    ResonariumProcessor& proc;
    MSEGParams msegParams;
    gin::MSEGComponent* msegComponent;
    gin::ParamComponent::Ptr r = nullptr;
    gin::ParamComponent::Ptr b = nullptr;
};

class ModSrcBox : public gin::ParamBox
{
public:
    ModSrcBox(const juce::String& name, ResonariumProcessor& proc)
        : gin::ParamBox(name), proc(proc)
    {
        setName("mod");

        addHeader({"SOURCES", "MATRIX"}, 0, proc.uiParams.modWindowSelect);
        headerTabButtonWidth = 100;
        addControl(modSrcListComponent = new gin::ModSrcListBox(proc.modMatrix));
    }

    void resized() override
    {
        ParamBox::resized();
        modSrcListComponent->setBounds(getLocalBounds().withTrimmedTop(BOX_HEADER_HEIGHT));
    }

    gin::ModSrcListBox* modSrcListComponent = nullptr;
    ResonariumProcessor& proc;
};

//==============================================================================
class MatrixBox : public gin::ParamBox
{
public:
    MatrixBox(const juce::String& name, ResonariumProcessor& proc)
        : gin::ParamBox(name), proc(proc)
    {
        setName("mtx");
        addHeader({"SOURCES", "MATRIX"}, 1, proc.uiParams.modWindowSelect);
        headerTabButtonWidth = 100;
        addControl(modMatrixComponent = new gin::ModMatrixBox(proc, proc.modMatrix, 75));
    }

    void resized() override
    {
        ParamBox::resized();
        modMatrixComponent->setBounds(getLocalBounds().withTrimmedTop(BOX_HEADER_HEIGHT));
    }

    gin::ModMatrixBox* modMatrixComponent = nullptr;
    ResonariumProcessor& proc;
};

class ChorusParamBox : public gin::ParamBox
{
public:
    ChorusParamBox(const juce::String& name, ResonariumProcessor& proc, ChorusParams chorusParams) :
        gin::ParamBox(name), proc(proc), chorusParams(chorusParams)
    {
        setName("chorus");
        addEnable(chorusParams.enabled);
        addControl(new gin::Select(chorusParams.sync), 0, 0);
        addControl(r = new gin::Knob(chorusParams.rate), 1, 0);
        addControl(b = new gin::Select(chorusParams.beat), 1, 0);
        addControl(new gin::Knob(chorusParams.depth), 2, 0);
        addControl(new gin::Knob(chorusParams.feedback), 0, 1);
        addControl(new gin::Knob(chorusParams.delay), 1, 1);
        addControl(new gin::Knob(chorusParams.mix), 2, 1);

        watchParam(chorusParams.sync);
    }

    ChorusParams chorusParams;
    ResonariumProcessor& proc;

    gin::Knob* r = nullptr;
    gin::Select* b = nullptr;

    void paramChanged() override
    {
        gin::ParamBox::paramChanged();
        if (r && b)
        {
            r->setVisible(!chorusParams.sync->isOn());
            b->setVisible(chorusParams.sync->isOn());
        }
    }
};

class PhaserParamBox : public gin::ParamBox
{
public:
    PhaserParamBox(const juce::String& name, ResonariumProcessor& proc, PhaserParams phaserParams) :
        gin::ParamBox(name), proc(proc), phaserParams(phaserParams)
    {
        setName("phaser");
        addEnable(phaserParams.enabled);
        addControl(new gin::Select(phaserParams.sync), 0, 0);
        addControl(r = new gin::Knob(phaserParams.rate), 1, 0);
        addControl(b = new gin::Select(phaserParams.beat), 1, 0);
        addControl(new gin::Knob(phaserParams.depth), 2, 0);
        addControl(new gin::Knob(phaserParams.feedback), 0, 1);
        addControl(new gin::Knob(phaserParams.centreFrequency), 1, 1);
        addControl(new gin::Knob(phaserParams.mix), 2, 1);

        watchParam(phaserParams.sync);
    }

    ResonariumProcessor& proc;
    PhaserParams phaserParams;

    gin::Knob* r;
    gin::Select* b;

    void paramChanged() override
    {
        gin::ParamBox::paramChanged();
        if (r && b)
        {
            r->setVisible(!phaserParams.sync->isOn());
            b->setVisible(phaserParams.sync->isOn());
        }
    }
};

class ReverbParamBox : public gin::ParamBox
{
public:
    ReverbParamBox(const juce::String& name, ResonariumProcessor& proc, ReverbParams reverbParams) :
        gin::ParamBox(name), proc(proc), reverbParams(reverbParams)
    {
        setName("reverb");
        addEnable(reverbParams.enabled);
        addControl(new gin::Knob(reverbParams.roomSize), 0, 0);
        addControl(new gin::Knob(reverbParams.width), 1, 0);
        addControl(new gin::Knob(reverbParams.damping), 2, 0);
        addControl(new gin::Knob(reverbParams.mix), 3, 0);
    }

    ResonariumProcessor& proc;
    ReverbParams reverbParams;
};

#endif //PANELS_H
