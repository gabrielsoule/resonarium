/**
* Collection of commonly used UI components
 */
#ifndef RESONARIUMCOMPONENTS_H
#define RESONARIUMCOMPONENTS_H

#include <JuceHeader.h>

#include "TextSlider.h"
#include "../Parameters.h"
#include "../defines.h"

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
    explicit WaveguideResonatorComponent_V2(ResonatorParams resonatorParams, juce::Colour colour) :
        resonatorParams(resonatorParams), colour(colour)
    {
        enableButton = new gin::SVGPluginButton(resonatorParams.enabled, gin::Assets::power);
        enableButton->setColour(juce::TextButton::buttonOnColourId, colour);
        enableButton->setColour(juce::TextButton::buttonColourId, colour);
        gainKnob = new gin::Knob(resonatorParams.gain);
        gainKnob->getSlider().setColour(juce::Slider::rotarySliderFillColourId, colour);
        gainKnob->getSlider().setTooltip("est");
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
        loopFilterModeKnob->getSlider().setTooltip(
            "Determines the type of filter used in the waveguide loop. 0 corresponds to a lowpass filter, 0.5 corresponds to a bandpass filter, and 1 corresponds to a highpass filter. Intermediate values smoothly interpolate between the filter modes");
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
        gainKnob->setBounds(
            bounds.removeFromTop(KNOB_W_SMALL).withWidth(KNOB_W_SMALL).translated(
                bounds.getWidth() / 2 - KNOB_W_SMALL / 2, 0));
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

class CircleOnOffButton : gin::PluginButton
{

};



#endif //RESONARIUMCOMPONENTS_H
