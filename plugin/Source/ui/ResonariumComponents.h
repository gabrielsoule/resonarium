/**
* Collection of commonly used UI components
 */
#ifndef RESONARIUMCOMPONENTS_H
#define RESONARIUMCOMPONENTS_H

#include <JuceHeader.h>

#include "ResonariumLookAndFeel.h"
#include "TextSlider.h"
#include "../Parameters.h"
#include "../defines.h"
#include "../ResonatorVoice.h"

// class ResonatorParameterRow : public gin::MultiParamComponent
// {
// public:
//     ResonatorParameterRow(juce::String title, gin::Parameter::Ptr paramArray[], size_t numParams, int parameterWidth, int parameterHeight, int padding): paramArray(paramArray), numParams(numParams)
//     {
//         for (int i = 0; i < numParams; i++)
//         {
//             auto* control = new TextSlider(paramArray[i]);
//             addAndMakeVisible(control);
//             watchParam(paramArray[i]);
//
//         }
//     }
//
//     gin::Parameter::Ptr* paramArray;
//     int numParams;
//     int parameterWidth;
//     int parameterHeight;
//     int padding;
// };

static constexpr float SPACING_X = 10; // how far apart are resonator columns?
static constexpr float SPACING_Y_SMALL = 1;
static constexpr float SPACING_Y_LARGE = 10;
static constexpr float PARAMETER_WIDTH = 68;
static constexpr float PARAMETER_HEIGHT = 25;
static constexpr float PARAMETER_TEXT_HEIGHT = 14;


class WaveguideResonatorComponent_V2 : public gin::MultiParamComponent
{
public:

    explicit WaveguideResonatorComponent_V2(ResonatorParams resonatorParams, juce::Colour colour) : resonatorParams(resonatorParams), colour(colour)
    {
        enableButton = new gin::SVGPluginButton(resonatorParams.enabled, gin::Assets::power);
        enableButton->setColour(juce::TextButton::buttonOnColourId, colour);
        gainKnob = new gin::Knob(resonatorParams.gain);
        gainKnob->getSlider().setColour(juce::Slider::rotarySliderFillColourId, colour);
        pitchOffsetKnob = new TextSlider(resonatorParams.harmonicInSemitones, colour);
        pitchOffsetKnob->setReadoutDecimals(2);
        decayTimeKnob = new TextSlider(resonatorParams.decayTime, colour);
        decayTimeKnob->setReadoutDecimals(1);
        dispersionKnob = new TextSlider(resonatorParams.dispersion, colour);
        dispersionKnob->setReadoutDecimals(0);
        loopFilterCutoffKnob = new TextSlider(resonatorParams.loopFilterCutoff, colour);
        loopFilterCutoffKnob->setReadoutDecimals(0);
        loopFilterResonanceKnob = new TextSlider(resonatorParams.loopFilterResonance, colour);
        loopFilterResonanceKnob->setReadoutDecimals(2);
        loopFilterModeKnob = new TextSlider(resonatorParams.loopFilterMode, colour);
        loopFilterModeKnob->setReadoutDecimals(2);
        postFilterCutoffKnob = new TextSlider(resonatorParams.postFilterCutoff, colour);
        postFilterCutoffKnob->setReadoutDecimals(0);
        postFilterResonanceKnob = new TextSlider(resonatorParams.postFilterResonance, colour);
        postFilterResonanceKnob->setReadoutDecimals(2);
        postFilterModeKnob = new TextSlider(resonatorParams.postFilterMode, colour);
        postFilterModeKnob->setReadoutDecimals(2);


        addAndMakeVisible(enableButton);
        addAndMakeVisible(gainKnob);
        addAndMakeVisible(pitchOffsetKnob);
        addAndMakeVisible(decayTimeKnob);
        addAndMakeVisible(dispersionKnob);
        addAndMakeVisible(loopFilterCutoffKnob);
        addAndMakeVisible(loopFilterResonanceKnob);
        addAndMakeVisible(loopFilterModeKnob);
        addAndMakeVisible(postFilterCutoffKnob);
        addAndMakeVisible(postFilterResonanceKnob);
        addAndMakeVisible(postFilterModeKnob);

        watchParam(enableButton->parameter);
    }

    void resized() override
    {
        auto bounds = this->getLocalBounds();
        enableButton->setBounds(bounds.removeFromTop(12));
        bounds.removeFromTop(3);
        gainKnob->setBounds(bounds.removeFromTop(KNOB_W_SMALL).withWidth(KNOB_W_SMALL).translated(bounds.getWidth() / 2 - KNOB_W_SMALL / 2, 0));
        bounds.removeFromTop(SPACING_Y_SMALL);
        pitchOffsetKnob->setBounds(bounds.removeFromTop(PARAMETER_HEIGHT));
        bounds.removeFromTop(SPACING_Y_LARGE);
        decayTimeKnob->setBounds(bounds.removeFromTop(PARAMETER_HEIGHT));
        bounds.removeFromTop(SPACING_Y_LARGE);
        dispersionKnob->setBounds(bounds.removeFromTop(PARAMETER_HEIGHT));
        bounds.removeFromTop(SPACING_Y_LARGE);
        loopFilterCutoffKnob->setBounds(bounds.removeFromTop(PARAMETER_HEIGHT));
        bounds.removeFromTop(SPACING_Y_SMALL);
        loopFilterResonanceKnob->setBounds(bounds.removeFromTop(PARAMETER_HEIGHT));
        bounds.removeFromTop(SPACING_Y_SMALL);
        loopFilterModeKnob->setBounds(bounds.removeFromTop(PARAMETER_HEIGHT));
        bounds.removeFromTop(SPACING_Y_LARGE);
        postFilterCutoffKnob->setBounds(bounds.removeFromTop(PARAMETER_HEIGHT));
        bounds.removeFromTop(SPACING_Y_SMALL);
        postFilterResonanceKnob->setBounds(bounds.removeFromTop(PARAMETER_HEIGHT));
        bounds.removeFromTop(SPACING_Y_SMALL);
        postFilterModeKnob->setBounds(bounds.removeFromTop(PARAMETER_HEIGHT));


    }
    void paramChanged() override
    {
        // for some reason paramChanged() is being called before this component is added to the parent
        // this is fine, except that we can't access the lookandfeel at this point
        // this is a hack to get around that. There might be side effects. That's a later problem.
        if (this->getParentComponent() == nullptr)
        {
            return;
        }

        MultiParamComponent::paramChanged();
        for (auto c : this->getChildren())
        {
            if (c != enableButton) c->setEnabled(resonatorParams.enabled->isOn());
        }
    }

    gin::SVGPluginButton* enableButton;
    gin::Knob* gainKnob;
    TextSlider* pitchOffsetKnob;
    TextSlider* decayTimeKnob;
    TextSlider* dispersionKnob;
    TextSlider* loopFilterCutoffKnob;
    TextSlider* loopFilterResonanceKnob;
    TextSlider* loopFilterModeKnob;
    TextSlider* postFilterCutoffKnob;
    TextSlider* postFilterResonanceKnob;
    TextSlider* postFilterModeKnob;

    ResonatorParams resonatorParams;
    juce::Colour colour;

    int backgroundRectangleStartY;
};

class WaveguideResonatorComponent : public gin::MultiParamComponent
{
public:
    WaveguideResonatorComponent(ResonatorParams resonatorParams) : resonatorParams(resonatorParams)
    {
        this->enableButton = new gin::SVGPluginButton(resonatorParams.enabled, gin::Assets::power);
        this->gainKnob = new TextSlider(resonatorParams.gain);
        this->pitchOffsetKnob = new gin::Knob(resonatorParams.harmonicInSemitones);
        this->dispersionKnob = new gin::Knob(resonatorParams.dispersion);
        this->decayTimeKnob = new gin::Knob(resonatorParams.decayTime);
        // this->gainKnob->getSlider().getProperties().set("textOnly", true);
        // this->gainKnob->setName("MyGainKnob");
        jassert(this->gainKnob->getSlider().getProperties()["textOnly"]);
        // this->gainKnob->getSlider().setInterceptsMouseClicks(false, true);
        // this->brightnessKnob = new gin::Knob(resonatorParams.eksFilterBrightness);
        // this->decayFilterCutoffKnob = new gin::Knob(resonatorParams.decayFilterCutoff);
        this->svfCutoffKnob = new gin::Knob(resonatorParams.loopFilterCutoff);
        this->svfResonanceKnob = new gin::Knob(resonatorParams.loopFilterResonance);

        addAndMakeVisible(enableButton);
        addAndMakeVisible(gainKnob);
        addAndMakeVisible(pitchOffsetKnob);
        addAndMakeVisible(decayTimeKnob);
        addAndMakeVisible(dispersionKnob);
        addAndMakeVisible(svfCutoffKnob);
        addAndMakeVisible(svfResonanceKnob);
        // addAndMakeVisible(brightnessKnob);
        // addAndMakeVisible(decayFilterCutoffKnob);

        watchParam(resonatorParams.enabled);

        setSize(KNOB_W_SMALL, 300);
    }

    void resized() override
    {
        auto bounds = getLocalBounds();
        bounds.removeFromTop(7); //just a little off the top, please
        enableButton->setBounds(bounds.removeFromTop(12));
        gainKnob->setBounds(bounds.removeFromTop(KNOB_H_SMALL));
        pitchOffsetKnob->setBounds(bounds.removeFromTop(KNOB_H_SMALL));
        decayTimeKnob->setBounds(bounds.removeFromTop(KNOB_H_SMALL));
        dispersionKnob->setBounds(bounds.removeFromTop(KNOB_H_SMALL));
        svfCutoffKnob->setBounds(bounds.removeFromTop(KNOB_H_SMALL));
        svfResonanceKnob->setBounds(bounds.removeFromTop(KNOB_H_SMALL));
        // brightnessKnob->setBounds(bounds.removeFromTop(KNOB_H_SMALL));
        // decayFilterCutoffKnob->setBounds(bounds.removeFromTop(KNOB_H_SMALL));
    }

    void paramChanged() override
    {
        // for some reason paramChanged() is being called before this component is added to the parent
        // this is fine, except that we can't access the lookandfeel at this point
        // this is a hack to get around that. There might be side effects. That's a later problem.
        if (this->getParentComponent() == nullptr)
        {
            return;
        }

        MultiParamComponent::paramChanged();
        for (auto c : this->getChildren())
        {
            if (c != enableButton) c->setEnabled(resonatorParams.enabled->isOn());
        }

        if (resonatorParams.enabled->isOn())
        {
            borderColor = findColour(gin::PluginLookAndFeel::accentColourId, true);
        }
        else
        {
            borderColor = findColour(gin::PluginLookAndFeel::title1ColourId, true);
        }
    }

    void paint(juce::Graphics& g) override
    {
        g.setColour(borderColor); // Set the color of the background
        float cornerSize = 10.0f; // Set the corner size for the rounded rectangle
        // g.drawRoundedRectangle (getLocalBounds().reduced(2.0f,2.0f).toFloat(), cornerSize, 2); // Draw the rounded rectangle
    }

    const float padding = 4.0f;
    juce::Colour borderColor;
    gin::SVGPluginButton* enableButton;
    gin::Knob* gainKnob;
    gin::Knob* pitchOffsetKnob;
    gin::Knob* decayTimeKnob;
    gin::Knob* dispersionKnob;
    gin::Knob* decayFilterCutoffKnob;
    gin::Knob* brightnessKnob;
    gin::Knob* svfCutoffKnob;
    gin::Knob* svfResonanceKnob;

    ResonatorParams resonatorParams;
};

/**
 * Component containing a vertical stack of controls corresponding to a single modal filter.
 * Very similar to WaveguideResonatorComponent, but at this stage of development I'd rather have a
 * little bit of duplicated code rather than a complex polymorphic "GeneralizedResonatorKnobStack" virtual
 * class or something like that...
 */
class ModalResonatorComponent : public gin::MultiParamComponent
{
public:
    ModalResonatorComponent(ModalResonatorBankParams params, int index) : resonatorParams(params), index(index)
    {
        this->enableButton = new gin::SVGPluginButton(resonatorParams.enabled[index], gin::Assets::power);
        this->gainKnob = new gin::Knob(resonatorParams.gain[index]);
        this->pitchOffsetKnob = new gin::Knob(resonatorParams.harmonicInSemitones[index]);
        this->decayTimeKnob = new gin::Knob(resonatorParams.decay[index]);

        addAndMakeVisible(enableButton);
        addAndMakeVisible(gainKnob);
        addAndMakeVisible(pitchOffsetKnob);
        addAndMakeVisible(decayTimeKnob);

        watchParam(resonatorParams.enabled[index]);
    }

    void resized() override
    {
        auto bounds = getLocalBounds();
        bounds.removeFromTop(7); //just a little off the top, please
        enableButton->setBounds(bounds.removeFromTop(12));
        gainKnob->setBounds(bounds.removeFromTop(KNOB_H_SMALL));
        pitchOffsetKnob->setBounds(bounds.removeFromTop(KNOB_H_SMALL));
        decayTimeKnob->setBounds(bounds.removeFromTop(KNOB_H_SMALL));
    }

    void paramChanged() override
    {
        if (this->getParentComponent() == nullptr)
        {
            return;
        }

        MultiParamComponent::paramChanged();
        for (auto c : this->getChildren())
        {
            if (c != enableButton) c->setEnabled(resonatorParams.enabled[index]->isOn());
        }
    }

    const float padding = 4.0f;
    gin::SVGPluginButton* enableButton;
    gin::Knob* gainKnob;
    gin::Knob* pitchOffsetKnob;
    gin::Knob* decayTimeKnob;

    ModalResonatorBankParams resonatorParams;
    int index;
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
        gridWidth = gridWidth * 0.9f;
        gridHeight = gridHeight * 0.9f;
        gridOffsetY += 4;
        addControl(new gin::Knob(sampleParams.gain), 0, 0);
        addControl(new gin::Knob(sampleParams.mix), 1, 0);
        addControl(new gin::Knob(sampleParams.start), 2, 0);
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
        resonatorsArea.setHeight(350);
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
        for(int i = 0; i < NUM_WAVEGUIDE_RESONATORS; i++)
        {
            resonatorColors.set(i, resonatorColors[i].withSaturation(1.0).withLightness(0.8));
        }
        setName("waveguideResonatorBankParams " + juce::String(resonatorNum));
        this->headerTabButtonWidth = 150;
        addHeader({"MODAL 1", "WAVEGUIDE 1", "MODAL 2", "WAVEGUIDE 2"}, resonatorNum, uiParams.resonatorBankSelect);
        auto* select = new gin::Select(bankParams.couplingMode);
        addControl(select);
        for (int i = 0; i < NUM_WAVEGUIDE_RESONATORS; i++)
        {
            auto* resonatorComponent = new WaveguideResonatorComponent_V2(
                bankParams.resonatorParams[i], resonatorColors[i]);
            resonatorComponents.add(resonatorComponent);
            addAndMakeVisible(resonatorComponent);
        }

        textColour = juce::Colours::white.withAlpha(0.5f);

        addAndMakeVisible(loopFilterLabel = new juce::Label("loopFilterLabel", "LOOP\nFILTER"));
        addAndMakeVisible(postFilterLabel = new juce::Label("postFilterLabel", "POST\nFILTER"));
        loopFilterLabel->setJustificationType(juce::Justification::centred);
        postFilterLabel->setJustificationType(juce::Justification::centred);
        loopFilterLabel->setFont(loopFilterLabel->getFont().withHeight(17).withExtraKerningFactor(0.05f));
        postFilterLabel->setFont(postFilterLabel->getFont().withHeight(17).withExtraKerningFactor(0.05f));
        loopFilterLabel->setEditable(false);
        postFilterLabel->setEditable(false);
        loopFilterLabel->setColour(juce::Label::textColourId, textColour);
        postFilterLabel->setColour(juce::Label::textColourId, textColour);
    }

    void resized() override
    {
        ParamBox::resized();
        juce::Rectangle<int> resonatorsArea = getLocalBounds();
        resonatorsArea.removeFromTop(BOX_HEADER_HEIGHT + 10);
        resonatorsArea.setHeight(350);
        resonatorsArea.removeFromLeft(135);
        for (int i = 0; i < NUM_WAVEGUIDE_RESONATORS; i++)
        {
            resonatorsArea.removeFromLeft(7);
            resonatorComponents[i]->setBounds(resonatorsArea.removeFromLeft(55));
        }
        loopFilterLabel->setBounds(-1, 220, 65, 40);
        loopFilterLabel->setTransform(juce::AffineTransform::rotation(-juce::MathConstants<float>::halfPi, loopFilterLabel->getBounds().getCentreX(), loopFilterLabel->getBounds().getCentreY()));
        // loopFilterLabel->setTransform(juce::AffineTransform::rotation(juce::MathConstants<float>::halfPi));
        loopFilterLabel->toFront(false);

        postFilterLabel->setBounds(-1, 310, 65, 40);
        postFilterLabel->setTransform(juce::AffineTransform::rotation(-juce::MathConstants<float>::halfPi, postFilterLabel->getBounds().getCentreX(), postFilterLabel->getBounds().getCentreY()));
        postFilterLabel->toFront(false);
    }

    void paint(juce::Graphics& g) override
    {
        ParamBox::paint(g);
        juce::Font font = static_cast<ResonariumLookAndFeel&>(getLookAndFeel()).defaultFont.withHeight(17).withExtraKerningFactor(0.05f);
        g.setFont(font);
        //draw some attractive background rectangles
        juce::Rectangle<float> rowBackground = juce::Rectangle<float>(137, BOX_HEADER_HEIGHT + 10 + 55 + 3, 498, PARAMETER_HEIGHT).expanded(0, 1);
        juce::Rectangle<int> textRect = juce::Rectangle<int>(rowBackground.getX() - 115, rowBackground.getY() + 1, 105, PARAMETER_HEIGHT);

        g.setColour(textColour);
        g.drawFittedText("GAIN", textRect.translated(0, -40), juce::Justification::centredRight, 1);

        g.setColour(juce::Colours::black);
        g.fillRoundedRectangle(rowBackground, 14);
        g.setColour(textColour);
        g.drawFittedText("PITCH", textRect, juce::Justification::centredRight, 1);

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
        juce::Rectangle filterBlockBackground = juce::Rectangle<float>(rowBackground.getX(), rowBackground.getY() + PARAMETER_HEIGHT + SPACING_Y_LARGE + 1, 498, 3 * PARAMETER_HEIGHT + 2 * SPACING_Y_SMALL).expanded(0, 1);
        g.fillRoundedRectangle(filterBlockBackground, 14);
        textRect.translate(0, PARAMETER_HEIGHT + SPACING_Y_LARGE);
        g.setColour(textColour);
        g.drawFittedText("CUTOFF", textRect, juce::Justification::centredRight, 1);
        textRect.translate(0, PARAMETER_HEIGHT + SPACING_Y_SMALL);
        g.drawFittedText("RES.", textRect, juce::Justification::centredRight, 1);
        textRect.translate(0, PARAMETER_HEIGHT + SPACING_Y_SMALL);
        g.drawFittedText("MODE", textRect, juce::Justification::centredRight, 1);
        juce::Path loopFilterBracketPath;
        loopFilterBracketPath.startNewSubPath(filterBlockBackground.getTopLeft().translated(-70, 2));
        loopFilterBracketPath.lineTo(filterBlockBackground.getTopLeft().translated(-80, 2));
        loopFilterBracketPath.lineTo(filterBlockBackground.getBottomLeft().translated(-80, -2));
        loopFilterBracketPath.lineTo(filterBlockBackground.getBottomLeft().translated(-70, -2));
        g.strokePath(loopFilterBracketPath, juce::PathStrokeType(2.0f));

        g.setColour(juce::Colours::black);
        filterBlockBackground.translate(0, 3 * PARAMETER_HEIGHT + 2 * SPACING_Y_SMALL + SPACING_Y_LARGE);
        g.fillRoundedRectangle(filterBlockBackground, 14);
        textRect.translate(0, SPACING_Y_LARGE + PARAMETER_HEIGHT);
        g.setColour(textColour);
        g.drawFittedText("CUTOFF", textRect, juce::Justification::centredRight, 1);
        textRect.translate(0, PARAMETER_HEIGHT + SPACING_Y_SMALL);
        g.drawFittedText("RES", textRect, juce::Justification::centredRight, 1);
        textRect.translate(0, PARAMETER_HEIGHT + SPACING_Y_SMALL);
        g.drawFittedText("MODE", textRect, juce::Justification::centredRight, 1);
        juce::Path postFilterBracketPath;
        postFilterBracketPath.startNewSubPath(filterBlockBackground.getTopLeft().translated(-70, 2));
        postFilterBracketPath.lineTo(filterBlockBackground.getTopLeft().translated(-80, 2));
        postFilterBracketPath.lineTo(filterBlockBackground.getBottomLeft().translated(-80, -2));
        postFilterBracketPath.lineTo(filterBlockBackground.getBottomLeft().translated(-70, -2));
        g.strokePath(postFilterBracketPath, juce::PathStrokeType(2.0f));


    }

    int resonatorBankIndex;
    WaveguideResonatorBankParams bankParams;
    juce::OwnedArray<WaveguideResonatorComponent_V2> resonatorComponents;
    UIParams uiParams;

    juce::Colour textColour;
    juce::Label* loopFilterLabel;
    juce::Label* postFilterLabel;
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
        resonatorsArea.setHeight(350);
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
        auto g = new gin::ADSRComponent();
        g->setParams(adsrParams.attack, adsrParams.decay, adsrParams.sustain, adsrParams.release);
        g->phaseCallback = [this, adsrParams]
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
        addControl(g, 4, 0, 3, 1);
    }

    ResonariumProcessor& proc;
    gin::ADSRComponent* visualizer;
    ADSRParams adsrParams;
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

        addControl(r = new gin::Knob(lfoParams.rate), 0, 0);
        addControl(b = new gin::Select(lfoParams.beat), 0, 0);
        addControl(new gin::Knob(lfoParams.stereo), 1, 0);
        addControl(new gin::Knob(lfoParams.depth, true), 2, 0);
        // addControl(new gin::Knob(lfoParams.fade, true), 3, 0);
        addControl(new gin::Knob(lfoParams.delay), 3, 0);

        addControl(new gin::Select(lfoParams.wave), 0, 1);
        addControl(new gin::Switch(lfoParams.sync), 1, 1);
        addControl(new gin::Knob(lfoParams.phase, true), 2, 1);
        addControl(new gin::Knob(lfoParams.offset, true), 3, 1);

        visualizer = new gin::LFOComponent();
        visualizer->setParams(lfoParams.wave, lfoParams.sync, lfoParams.rate, lfoParams.beat, lfoParams.depth,
                              lfoParams.offset,
                              lfoParams.phase, lfoParams.enabled);
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
        visualizer->setBounds(230, 40, 150, 110);
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

        addControl(r = new gin::Knob(randomLfoParams.rate), 0, 0);
        addControl(b = new gin::Select(randomLfoParams.beat), 0, 0);
        addControl(new gin::Knob(randomLfoParams.stereo), 1, 0);
        addControl(new gin::Knob(randomLfoParams.depth, true), 2, 0);
        addControl(new gin::Knob(randomLfoParams.jitter), 3, 0);
        addControl(new gin::Knob(randomLfoParams.chaos), 0, 1);
        addControl(new gin::Switch(randomLfoParams.sync), 1, 1);
        addControl(new gin::Knob(randomLfoParams.smooth), 2, 1);
        addControl(new gin::Knob(randomLfoParams.offset), 3, 1);
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
        addControl(new gin::Switch(msegParams.sync), 0, 2);
        addControl(r = new gin::Knob(msegParams.rate), 1, 2);
        addControl(b = new gin::Select(msegParams.beat), 1, 2);
        addControl(new gin::Knob(msegParams.xgrid), 5, 2);
        addControl(new gin::Knob(msegParams.ygrid, true), 6, 2);


        msegComponent = new gin::MSEGComponent(proc.synth.monoMSEGs.getReference(msegParams.index).leftData);
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
                      .reduced(2, BOX_HEADER_HEIGHT + 2)
                      .withTrimmedBottom(55);
        msegComponent->setBounds(bounds);
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

class TestParamBox : public gin::ParamBox
{
public:
    TestParamBox(const juce::String& name, ResonariumProcessor& proc) : gin::ParamBox(name), mseg(data)
    {
        msegComponent = new gin::MSEGComponent(data);
        msegComponent->setBounds(50, 50, 300, 200);
        addControl(msegComponent);
    }

    gin::MSEGComponent* msegComponent;
    gin::MSEG::Data data;
    gin::MSEG mseg;
};


#endif //RESONARIUMCOMPONENTS_H
