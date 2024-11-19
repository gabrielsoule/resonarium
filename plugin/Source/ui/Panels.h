#ifndef PANELS_H
#define PANELS_H

#include <JuceHeader.h>

#include "ClickThruSelect.h"
#include "RandomLFOComponent.h"
#include "TextSlider.h"
#include "../Parameters.h"
#include "../defines.h"
#include "ResonariumComponents.h"
#include "SampleDropperComponent.h"
#include "../ResonatorVoice.h"

class GraphicsUtils
{
public:
    static void drawBracketLeft(const juce::Graphics& g, float x, float startY, float endY)
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

        addControl(sampleDropper = new SampleDropperComponent(proc.sampler), 0, 0, 4, 1);

        addControl(new gin::Knob(sampleParams.level), 0, 1);
        addControl(new gin::Knob(sampleParams.mix), 1, 1);
        addControl(new gin::Knob(sampleParams.start), 2, 1);
        addControl(new gin::Switch(sampleParams.loop), 3, 1);

        addControl(new gin::Knob(sampleParams.adsrParams.attack), 0, 2);
        addControl(new gin::Knob(sampleParams.adsrParams.decay), 1, 2);
        addControl(new gin::Knob(sampleParams.adsrParams.sustain), 2, 2);
        addControl(new gin::Knob(sampleParams.adsrParams.release), 3, 2);

        addControl(new gin::Select(sampleParams.filterParams.type), 1, 3);
        addControl(new gin::Knob(sampleParams.filterParams.frequency), 2, 3);
        addControl(new gin::Knob(sampleParams.filterParams.resonance), 3, 3);
    }

    SampleExciterParams sampleParams;
    SampleDropperComponent* sampleDropper;
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
        jassert(resonatorColors.size() == NUM_RESONATORS);
        for (int i = 0; i < NUM_RESONATORS; i++)
        {
            resonatorColors.set(i, resonatorColors[i].withSaturation(1.0).withLightness(0.7));
        }
        setName("waveguideResonatorBankParams " + juce::String(resonatorNum));
        this->headerTabButtonWidth = 150;
        juce::StringArray headerButtonNames;
        for (int i = 0; i < NUM_RESONATOR_BANKS; i++)
        {
            headerButtonNames.add("WAVEGUIDE " + juce::String(i + 1));
        }
        addHeader(headerButtonNames, resonatorNum,
                  uiParams.resonatorBankSelect);
        for (int i = 0; i < NUM_RESONATORS; i++)
        {
            auto* resonatorComponent = new WaveguideResonatorComponent_V2(bankParams,
                                                                          bankParams.resonatorParams[i],
                                                                          resonatorColors[i]);
            resonatorComponents.add(resonatorComponent);
            addAndMakeVisible(resonatorComponent);
        }

        textColour = juce::Colours::white.withAlpha(0.5f);

        addAndMakeVisible(loopFilterLabel = new juce::Label("loopFilterLabel", "LOOP FILTER"));
        addAndMakeVisible(postFilterLabel = new juce::Label("postFilterLabel", "POST FILTER"));
        loopFilterLabel->setJustificationType(juce::Justification::centred);
        postFilterLabel->setJustificationType(juce::Justification::centred);
        loopFilterLabel->setFont(loopFilterLabel->getFont().withHeight(20).withExtraKerningFactor(0.06f));
        postFilterLabel->setFont(postFilterLabel->getFont().withHeight(20).withExtraKerningFactor(0.06f));
        loopFilterLabel->setEditable(false);
        postFilterLabel->setEditable(false);
        loopFilterLabel->setColour(juce::Label::textColourId, textColour);
        postFilterLabel->setColour(juce::Label::textColourId, textColour);

        showSemitonesToggle = new SVGFilePluginButton(bankParams.useSemitones, BinaryData::music_note_svg, BinaryData::music_note_svg);

        addControl(showSemitonesToggle);

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

        topControlBracketLabel = new juce::Label("topControlBracketLabel", "WAVEGUIDE\nCONTROLS");
        topControlBracketLabel->setJustificationType(juce::Justification::centred);
        topControlBracketLabel->setFont(topControlBracketLabel->getFont().withHeight(19).withExtraKerningFactor(0.06f));
        topControlBracketLabel->setEditable(false);
        topControlBracketLabel->setColour(juce::Label::textColourId, textColour);
        addAndMakeVisible(topControlBracketLabel);
        topControlBracket = new BracketComponent(BracketComponent::Orientation::top);
        addAndMakeVisible(topControlBracket);
        bottomControlBracket = new BracketComponent(BracketComponent::Orientation::bottom);
        addAndMakeVisible(bottomControlBracket);
        bottomControlBracketLabel = new juce::Label("bottomControlBracketLabel", "CASCADE\nCONTROLS");
        bottomControlBracketLabel->setJustificationType(juce::Justification::centred);
        bottomControlBracketLabel->setFont(
            bottomControlBracketLabel->getFont().withHeight(19).withExtraKerningFactor(0.06f));
        bottomControlBracketLabel->setEditable(false);
        bottomControlBracketLabel->setColour(juce::Label::textColourId, textColour);
        addAndMakeVisible(bottomControlBracketLabel);
        controlAreaGlow = new GlowComponent();
        addAndMakeVisible(controlAreaGlow);
        controlAreaGlow->toBack();

        watchParam(bankParams.couplingMode);
        watchParam(bankParams.useSemitones);
    }

    void resized() override
    {
        ParamBox::resized();
        juce::Rectangle<int> resonatorsArea = getLocalBounds();
        resonatorsArea.removeFromTop(BOX_HEADER_HEIGHT + 10);
        resonatorsArea.setHeight(RESONATOR_BANK_BOX_HEIGHT);
        resonatorsArea.removeFromLeft(155);
        for (int i = 0; i < NUM_RESONATORS; i++)
        {
            resonatorComponents[i]->setBounds(resonatorsArea.removeFromLeft(62));
            resonatorsArea.removeFromLeft(4);
        }

        float bankControlsX = 710;
        float bankControlsY = BOX_HEADER_HEIGHT + 105;


        topControlBracket->setBounds(bankControlsX - 1, bankControlsY - 5, KNOB_W * 2 + 1, 10);
        topControlBracketLabel->setBounds(bankControlsX, bankControlsY - 50, KNOB_W * 2, 40);
        showSemitonesToggle->setBounds(85, 125 + 35, 60, 17);
        inputGainKnob->setBounds(bankControlsX, bankControlsY, KNOB_W, KNOB_H);
        inputMixKnob->setBounds(bankControlsX + KNOB_W, bankControlsY, KNOB_W, KNOB_H);
        outputGainKnob->setBounds(bankControlsX + KNOB_W * 0.5f, bankControlsY + KNOB_H, KNOB_W, KNOB_H);
        couplingModeKnob->setBounds(bankControlsX + KNOB_W * 0.5f, bankControlsY + 2 * KNOB_H, KNOB_W, KNOB_H);

        cascadeAmountKnob->setBounds(bankControlsX, bankControlsY + 3.18 * (KNOB_H), KNOB_W, KNOB_H);
        cascadeFilterKnob->setBounds(bankControlsX + KNOB_W, bankControlsY + 3.18 * (KNOB_H), KNOB_W, KNOB_H);
        cascadeFilterResonanceKnob->setBounds(bankControlsX, bankControlsY + 4.18 * (KNOB_H), KNOB_W, KNOB_H);
        cascadeFilterModeKnob->setBounds(bankControlsX + KNOB_W, bankControlsY + 4.18 * (KNOB_H), KNOB_W, KNOB_H);

        bottomControlBracket->setBounds(bankControlsX - 1, cascadeFilterModeKnob->getBounds().getBottom(),
                                        KNOB_W * 2 + 1, 10);
        bottomControlBracketLabel->setBounds(bankControlsX, bottomControlBracket->getBounds().getBottom() + 3,
                                             KNOB_W * 2, 40);

        controlAreaGlow->setBounds(juce::Rectangle<int>(topControlBracket->getBounds().getTopLeft(),
                                                        bottomControlBracket->getBounds().getBottomRight()).
            expanded(50));
        GlowComponent::GlowParameters glowParams;
        glowParams.colour = juce::Colour(0xff775cff);
        glowParams.radiusX = 20;
        glowParams.radiusY = 100;
        glowParams.opacity = 0.25;
        glowParams.spread = 0;
        glowParams.glowRadius = 80;
        controlAreaGlow->setParameters(glowParams);

        //it's really annoying to position rotated text components, since the affine transforms affect the coordinates...
        //we just do it "by hand" here which is... also really annoying
        loopFilterLabel->setBounds(-5, 295, 115, 30);
        loopFilterLabel->setTransform(juce::AffineTransform::rotation(-juce::MathConstants<float>::halfPi,
                                                                      loopFilterLabel->getBounds().getCentreX(),
                                                                      loopFilterLabel->getBounds().getCentreY()));
        // loopFilterLabel->setTransform(juce::AffineTransform::rotation(juce::MathConstants<float>::halfPi));
        loopFilterLabel->toFront(false);

        postFilterLabel->setBounds(-5, 410, 115, 30);
        postFilterLabel->setTransform(juce::AffineTransform::rotation(-juce::MathConstants<float>::halfPi,
                                                                      postFilterLabel->getBounds().getCentreX(),
                                                                      postFilterLabel->getBounds().getCentreY()));
        postFilterLabel->toFront(false);

        if(bankParams.index == 0) inputMixKnob->setEnabled(false);
    }

    void paramChanged() override
    {
        ParamBox::paramChanged();
        bool showCascadeControls = static_cast<int>(bankParams.couplingMode->getProcValue()) == 2;
        if (cascadeAmountKnob != nullptr)
            cascadeAmountKnob->setVisible(showCascadeControls);
        if (cascadeFilterKnob != nullptr)
            cascadeFilterKnob->setVisible(showCascadeControls);
        if (cascadeFilterResonanceKnob != nullptr)
            cascadeFilterResonanceKnob->setVisible(showCascadeControls);
        if (cascadeFilterModeKnob != nullptr)
            cascadeFilterModeKnob->setVisible(showCascadeControls);
        if (bottomControlBracket != nullptr)
            bottomControlBracket->setVisible(showCascadeControls);
        if (bottomControlBracketLabel != nullptr)
            bottomControlBracketLabel->setVisible(showCascadeControls);

        for(auto* resonatorComponent : resonatorComponents)
        {
            resonatorComponent->pitchKnob->mainReadout.valueUpdated(nullptr);
        }
    }

    void paint(juce::Graphics& g) override
    {
        ParamBox::paint(g);
        juce::Font font = static_cast<ResonariumLookAndFeel&>(getLookAndFeel()).defaultFont.withHeight(19).
            withExtraKerningFactor(0.05f);
        g.setFont(font);
        //draw some attractive background rectangles

        juce::Rectangle<float> rowBackground = juce::Rectangle<float>(157, BOX_HEADER_HEIGHT + 10 + 55 + 3 + 33, 518,
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
        if(showSemitonesToggle->isMouseOver()) g.setColour(textColour.withAlpha(0.7f));
        g.drawFittedText("PITCH", textRect, juce::Justification::centredRight, 1);
        rowBackground.translate(0, PARAMETER_HEIGHT + SPACING_Y_SMALL);
        g.setColour(textColour);

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

        GraphicsUtils::drawBracketLeft(g, filterBlockBackground.getTopLeft().x - 80,
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

        GraphicsUtils::drawBracketLeft(g, filterBlockBackground.getTopLeft().x - 80,
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

    gin::Select* couplingModeKnob = nullptr;
    gin::Knob* inputMixKnob = nullptr;
    gin::Knob* inputGainKnob = nullptr;
    gin::Knob* outputGainKnob = nullptr;
    gin::Knob* cascadeAmountKnob = nullptr;
    gin::Knob* cascadeFilterKnob = nullptr;
    gin::Knob* cascadeFilterResonanceKnob = nullptr;
    gin::Knob* cascadeFilterModeKnob = nullptr;

    SVGFilePluginButton* showSemitonesToggle = nullptr;

    juce::Label* topControlBracketLabel = nullptr;
    BracketComponent* topControlBracket = nullptr;
    BracketComponent* bottomControlBracket = nullptr;
    juce::Label* bottomControlBracketLabel = nullptr;
    GlowComponent* controlAreaGlow = nullptr;
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
        gin::ParamBox(name), proc(proc), randomLfoParams(randomLfoParams)
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
        addControl(new gin::Knob(randomLfoParams.chaos), 4, 0);
        addControl(new gin::Knob(randomLfoParams.smooth), 5, 0);

        visualizer = new RandomLFOComponent(randomLfoParams);
        addControl(visualizer);
        visualizer->stateCallback = [this, randomLfoParams]
        {
            return this->proc.synth.monoRandomLFOs[randomLfoParams.index].centerState.atomicState.load(std::memory_order_relaxed);
        };

        watchParam(randomLfoParams.sync);
    }

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
            r->setVisible(!randomLfoParams.sync->isOn());
            b->setVisible(randomLfoParams.sync->isOn());
        }
    }

    ResonariumProcessor& proc;
    RandomLFOParams randomLfoParams;
    gin::ParamComponent::Ptr r = nullptr;
    gin::ParamComponent::Ptr b = nullptr;
    RandomLFOComponent* visualizer;
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

class MacroParamBox : public gin::ParamBox
{
public:
    MacroParamBox(const juce::String& name, ResonariumProcessor& proc,
                  std::array<gin::Parameter::Ptr, NUM_MACROS> macroParams) :
        gin::ParamBox(name), proc(proc), macroParams(macroParams)
    {
        setName("macro");
        addHeader({"MACROS", "SOURCES", "MATRIX"}, 0, proc.uiParams.modWindowSelect);
        headerTabButtonWidth = 100;
        for (int i = 0; i < NUM_MACROS; i++)
        {
            addControl(knobs[i] = new gin::Knob(macroParams[i]));
            knobs[i]->getSlider().setTooltip(
                "Control the value of this macro's outgoing modulation signal.");
            // knobs[i]->getSlider().setTooltip("This is a test tooltip,it doesn't do much");f
            modSrcButtons[i] = new gin::ModulationSourceButton(proc.modMatrix, proc.modSrcMacro[i], false);
            frame.addAndMakeVisible(modSrcButtons[i]);
        }
    }

    void resized() override
    {
        ParamBox::resized();
        //Some simple code to lay out the knobs and mod source buttons in an evenly spaced row
        int macroKnobWidth = 57;
        int availableWidth = getWidth() - NUM_MACROS * macroKnobWidth;
        int knobSpacing = availableWidth / (NUM_MACROS + 1);
        int yOffset = 38;

        for (int i = 0; i < NUM_MACROS; i++)
        {
            knobs[i]->setBounds(knobSpacing * (i + 1) + macroKnobWidth * i, yOffset, macroKnobWidth, KNOB_H * 1.1);
            //center the 15x15 mod source button below the knob
            modSrcButtons[i]->setBounds(knobSpacing * (i + 1) + macroKnobWidth * i + macroKnobWidth / 2 - 7,
                                        KNOB_H * 1.1 + 10 + yOffset, 15, 15);
        }
    }

    ResonariumProcessor& proc;
    std::array<gin::Knob*, NUM_MACROS> knobs;
    std::array<gin::ModulationSourceButton*, NUM_MACROS> modSrcButtons;
    std::array<gin::Parameter::Ptr, NUM_MACROS> macroParams;
};

class ModSourceParamBox : public gin::ParamBox
{
public:
    ModSourceParamBox(const juce::String& name, ResonariumProcessor& proc)
        : gin::ParamBox(name), proc(proc)
    {
        setName("mod");

        addHeader({"MACROS", "SOURCES", "MATRIX"}, 1, proc.uiParams.modWindowSelect);
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

class MatrixParamBox : public gin::ParamBox
{
public:
    MatrixParamBox(const juce::String& name, ResonariumProcessor& proc)
        : gin::ParamBox(name), proc(proc)
    {
        setName("mtx");
        addHeader({"MACROS", "SOURCES", "MATRIX"}, 2, proc.uiParams.modWindowSelect);
        headerTabButtonWidth = 100;
        addControl(modMatrixComponent = new gin::ModMatrixBox(proc, proc.modMatrix, 50));
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
        addControl(new gin::Switch(chorusParams.sync), 0, 0);
        addControl(r = new gin::Knob(chorusParams.rate), 1, 0);
        addControl(b = new gin::Select(chorusParams.beat), 1, 0);
        addControl(new gin::Knob(chorusParams.depth), 2, 0);
        addControl(new gin::Knob(chorusParams.feedback), 0, 1);
        addControl(new gin::Knob(chorusParams.delay), 1, 1);
        addControl(new gin::Knob(chorusParams.mix), 2, 1);

        watchParam(chorusParams.sync);

        for (int i = 0; i < controls.size(); i++)
        {
            if (auto* knob = dynamic_cast<gin::Knob*>(controls[i]))
            {
                knob->setBounds(knob->getBounds().translated(KNOB_W * 0.5, 0));
            }

            if (auto* select = dynamic_cast<gin::Select*>(controls[i]))
            {
                select->setBounds(select->getBounds().translated(KNOB_W * 0.5, 0));
            }

            if (auto* switchControl = dynamic_cast<gin::Switch*>(controls[i]))
            {
                switchControl->setBounds(switchControl->getBounds().translated(KNOB_W * 0.5, 0));
            }
        }
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
        addControl(new gin::Switch(phaserParams.sync), 0, 0);
        addControl(r = new gin::Knob(phaserParams.rate), 1, 0);
        addControl(b = new gin::Select(phaserParams.beat), 1, 0);
        addControl(new gin::Knob(phaserParams.depth), 2, 0);
        addControl(new gin::Knob(phaserParams.feedback), 0, 1);
        addControl(new gin::Knob(phaserParams.centreFrequency), 1, 1);
        addControl(new gin::Knob(phaserParams.mix), 2, 1);

        watchParam(phaserParams.sync);

        for (int i = 0; i < controls.size(); i++)
        {
            if (auto* knob = dynamic_cast<gin::Knob*>(controls[i]))
            {
                knob->setBounds(knob->getBounds().translated(KNOB_W * 0.5, 0));
            }

            if (auto* select = dynamic_cast<gin::Select*>(controls[i]))
            {
                select->setBounds(select->getBounds().translated(KNOB_W * 0.5, 0));
            }

            if (auto* switchControl = dynamic_cast<gin::Switch*>(controls[i]))
            {
                switchControl->setBounds(switchControl->getBounds().translated(KNOB_W * 0.5, 0));
            }
        }
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

class CompressorParamBox : public gin::ParamBox
{
public:
    CompressorParamBox(const juce::String& name, ResonariumProcessor& proc, CompressorParams compressorParams) :
        gin::ParamBox(name), proc(proc), compressorParams(compressorParams)
    {
        addEnable(compressorParams.enabled);
        addControl(new gin::Knob(compressorParams.threshold), 0, 0);
        addControl(new gin::Knob(compressorParams.ratio), 1, 0);
        addControl(new gin::Knob(compressorParams.attack), 2, 0);
        addControl(new gin::Knob(compressorParams.release), 3, 0);
    }

    ResonariumProcessor& proc;
    CompressorParams compressorParams;
};

class ReverbParamBox : public gin::ParamBox
{
public:
    ReverbParamBox(const juce::String& name, ResonariumProcessor& proc, ReverbParams reverbParams) :
        gin::ParamBox(name), proc(proc), reverbParams(reverbParams)
    {
        setName("reverb");
        addEnable(reverbParams.enabled);
        addControl(new gin::Knob(reverbParams.size), 0, 0);
        addControl(new gin::Knob(reverbParams.density), 1, 0);
        addControl(new gin::Knob(reverbParams.decay), 2, 0);
        addControl(new gin::Knob(reverbParams.dampingFreq), 3, 0);
        addControl(new gin::Knob(reverbParams.bandwidthFreq), 0, 1);
        addControl(new gin::Knob(reverbParams.predelay), 1, 1);
        addControl(new gin::Knob(reverbParams.earlyMix), 2, 1);
        addControl(new gin::Knob(reverbParams.mix), 3, 1);
    }

    ResonariumProcessor& proc;
    ReverbParams reverbParams;
};

class DelayParamBox : public gin::ParamBox
{
public:
    DelayParamBox(const juce::String& name, ResonariumProcessor& proc, DelayParams delayParams) :
        gin::ParamBox(name), proc(proc), delayParams(delayParams)
    {
        setName("delay");
        gridWidth = KNOB_W_SMALL;
        gridHeight = KNOB_H_SMALL;
        gridOffsetY += 4;
        addEnable(delayParams.enabled);
        addControl(syncLKnob = new gin::Switch(delayParams.syncL), 0, 0);
        addControl(timeLKnob = new gin::Knob(delayParams.timeL), 1, 0);
        addControl(beatLKnob = new gin::Select(delayParams.beatL), 1, 0);
        addControl(lockKnob = new gin::Switch(delayParams.lock), 2, 0);
        addControl(timeLCopyKnob = new gin::Knob(delayParams.timeL), 3, 0);
        addControl(beatLCopyKnob = new gin::Select(delayParams.beatL), 3, 0);
        addControl(timeRKnob = new gin::Knob(delayParams.timeR), 3, 0);
        addControl(beatRKnob = new gin::Select(delayParams.beatR), 3, 0);
        addControl(syncRKnob = new gin::Switch(delayParams.syncR), 4, 0);
        addControl(new gin::Knob(delayParams.pingPongAmount), 1, 1);
        addControl(new gin::Knob(delayParams.feedback), 2, 1);
        addControl(new gin::Knob(delayParams.mix), 3, 1);

        watchParam(delayParams.syncL);
        watchParam(delayParams.syncR);
        watchParam(delayParams.lock);

        for (int i = 0; i < controls.size(); i++)
        {
            if (auto* knob = dynamic_cast<gin::Knob*>(controls[i]))
            {
                knob->internalKnobReduction = 0;
                knob->resized();
            }
        }
    }

    void paramChanged() override
    {
        gin::ParamBox::paramChanged();
        if (timeLKnob && timeLCopyKnob && timeRKnob && beatLKnob && beatLCopyKnob && beatRKnob)
        {
            timeLKnob->setVisible(!delayParams.syncL->isOn());
            timeLKnob->setDisplayName(delayParams.lock->isOn() ? "Time" : "Time L");
            beatLKnob->setVisible(delayParams.syncL->isOn());
            beatLKnob->setDisplayName(delayParams.lock->isOn() ? "Beat" : "Beat L");
            timeLCopyKnob->setVisible(delayParams.lock->isOn() && !delayParams.syncL->isOn());
            timeLCopyKnob->setDisplayName(delayParams.lock->isOn() ? "Time" : "Time L");
            beatLCopyKnob->setVisible(delayParams.lock->isOn() && delayParams.syncL->isOn());
            beatLCopyKnob->setDisplayName(delayParams.lock->isOn() ? "Beat" : "Beat L");
            timeRKnob->setVisible(!delayParams.lock->isOn() && !delayParams.syncR->isOn());
            timeRKnob->setDisplayName(delayParams.lock->isOn() ? "Time" : "Time R");
            beatRKnob->setVisible(!delayParams.lock->isOn() && delayParams.syncR->isOn());
            beatRKnob->setDisplayName(delayParams.lock->isOn() ? "Beat" : "Beat R");
        }
    }

    //we want to simulate the left and right knobs moving in concert when the lock toggle is on
    //to do this, we show and hide a copy of the left time or beat knob when the lock is on
    gin::Switch* syncLKnob;
    gin::Switch* syncRKnob;
    gin::Knob* timeLKnob;
    gin::Knob* timeLCopyKnob;
    gin::Knob* timeRKnob;
    gin::Select* beatLKnob;
    gin::Select* beatLCopyKnob;
    gin::Select* beatRKnob;
    gin::Switch* lockKnob;

    ResonariumProcessor& proc;
    DelayParams delayParams;
};

class DistortionParamBox : public gin::ParamBox
{
public:
    DistortionParamBox(const juce::String& name, ResonariumProcessor& proc, DistortionParams distortionParams) :
    gin::ParamBox(name), proc(proc), distortionParams(distortionParams)
    {
        setName("dist");
        addEnable(distortionParams.enabled);
        addControl(distortionModeKnob = new gin::Select(distortionParams.distortionMode), 0, 0);
        addControl(driveKnob = new gin::Knob(distortionParams.drive), 1, 0);
        addControl(outputGainKnob = new gin::Knob(distortionParams.outputGain), 2, 0);
        addControl(mixKnob = new gin::Knob(distortionParams.mix), 3, 0);
        addControl(filterPrePostKnob = new gin::Select(distortionParams.prePostFilter), 0, 1);
        addControl(filterCutoffKnob = new gin::Knob(distortionParams.cutoff), 1, 1);
        addControl(filterResonanceKnob = new gin::Knob(distortionParams.resonance), 2, 1);
        addControl(filterModeKnob = new gin::Knob(distortionParams.filterMode), 3, 1);

        watchParam(distortionParams.prePostFilter);
    }

    void paramChanged() override
    {
        ParamBox::paramChanged();
        bool filterEnabled = distortionParams.prePostFilter->getProcValue() != 0;
        filterCutoffKnob->setEnabled(filterEnabled);
        filterResonanceKnob->setEnabled(filterEnabled);
        filterModeKnob->setEnabled(filterEnabled);
    }

    gin::Select* distortionModeKnob;
    gin::Knob* driveKnob;
    gin::Knob* outputGainKnob;
    gin::Knob* mixKnob;
    gin::Select* filterPrePostKnob;
    gin::Knob* filterCutoffKnob;
    gin::Knob* filterResonanceKnob;
    gin::Knob* filterModeKnob;

    ResonariumProcessor& proc;
    DistortionParams distortionParams;
};

class MultiAmpParamBox : public gin::ParamBox
{
public:
    MultiAmpParamBox(const juce::String& name, ResonariumProcessor& proc, MultiAmpParams multiAmpParams) :
        gin::ParamBox(name), proc(proc), multiAmpParams(multiAmpParams)
    {
        setName("amp");
        addEnable(multiAmpParams.enabled);
        gridWidth = KNOB_W_SMALL;
        gridHeight = KNOB_H_SMALL;
        gridOffsetY += 4;
        addControl(new gin::Select(multiAmpParams.mode), 0, 0);
        addControl(paramAKnob = new gin::Knob(multiAmpParams.paramA), 1, 0);
        addControl(paramBKnob = new gin::Knob(multiAmpParams.paramB), 2, 0);
        addControl(paramCKnob = new gin::Knob(multiAmpParams.paramC), 3, 0);
        addControl(paramDKnob = new gin::Knob(multiAmpParams.paramD), 4, 0);

        watchParam(multiAmpParams.mode);

        for (int i = 0; i < controls.size(); i++)
        {
            if (auto* knob = dynamic_cast<gin::Knob*>(controls[i]))
            {
                knob->internalKnobReduction = 0;
                knob->resized();
            }
        }
    }

    void paramChanged() override
    {
        gin::ParamBox::paramChanged();
        const auto value = multiAmpParams.mode->getProcValue();
        const auto mode = static_cast<MultiAmp::Mode>(multiAmpParams.mode->getProcValue());
        paramAKnob->setDisplayName(MultiAmp::getParameterName(mode, 0));
        paramBKnob->setDisplayName(MultiAmp::getParameterName(mode, 1));
        paramCKnob->setDisplayName(MultiAmp::getParameterName(mode, 2));
        paramDKnob->setDisplayName(MultiAmp::getParameterName(mode, 3));
    }

    gin::Knob* paramAKnob;
    gin::Knob* paramBKnob;
    gin::Knob* paramCKnob;
    gin::Knob* paramDKnob;

    ResonariumProcessor& proc;
    MultiAmpParams multiAmpParams;
};

class SVFParamBox : public gin::ParamBox
{
public:
    SVFParamBox(const juce::String& name, ResonariumProcessor& proc, SVFParams svfParams) :
        gin::ParamBox(name), proc(proc), svfParams(svfParams)
    {
        setName(name);
        addEnable(svfParams.enabled);

        addControl(new gin::Knob(svfParams.mode), 0, 0);
        addControl(new gin::Knob(svfParams.cutoff), 1, 0);
        addControl(new gin::Knob(svfParams.resonance), 2, 0);

        for (int i = 0; i < controls.size(); i++)
        {
            if (auto* knob = dynamic_cast<gin::Knob*>(controls[i]))
            {
                knob->setBounds(knob->getBounds().translated(KNOB_W * 0.5, 0));
            }
        }
    }

    ResonariumProcessor& proc;
    SVFParams svfParams;
};

#endif //PANELS_H
