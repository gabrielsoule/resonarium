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

// inline juce::String pianoKeyboardSVG = "m79.398 7.1992h-58.797c-7.3984 0-13.5 6.1016-13.5 13.5v58.801c0 7.3984 6.1016 13.5 13.5 13.5h58.801c7.3984 0 13.5-6.1016 13.5-13.5l-0.003906-58.801c0-7.5-6-13.5-13.5-13.5zm-40.699 53.699c2.3008 0 4.3008-1.8984 4.3008-4.3008v-46.398h14v46.5c0 2.3008 1.8984 4.3008 4.3008 4.3008h2v29h-26.602v-29.102zm-28.598 18.602v-58.801c0-5.8008 4.6992-10.5 10.5-10.5h6.8008v46.5c0 2.3008 1.8984 4.3008 4.3008 4.3008h2l-0.003906 29h-13.098c-5.8008 0-10.5-4.6992-10.5-10.5zm79.797 0c0 5.8008-4.6992 10.5-10.5 10.5h-13.098v-29.102h2c2.3008 0 4.3008-1.8984 4.3008-4.3008v-46.398h6.8008c5.8008 0 10.5 4.6992 10.5 10.5z";
inline juce::String pianoKeyboardSVG =
    "m71.668 28.449h-43.336c-1.8359 0-3.332 1.4961-3.332 3.332v36.434c0 1.8359 1.4961 3.332 3.332 3.332h43.332c1.8398 0 3.332-1.4961 3.332-3.332l0.003906-36.43c0-1.8398-1.4961-3.3359-3.332-3.3359zm-31.395 38.934h-11.109l0.003907-34.766h8.3008v20.141h2.8047zm15.285 0h-11.121l0.003906-14.625h2.8047v-20.141h5.5078v20.141h2.8047zm15.273 0h-11.109l0.003906-14.625h2.8047v-20.141h8.3008z";
inline juce::String unlockSVG =
    "M4 30.016v-14.016q0-0.832 0.576-1.408t1.44-0.576v-4q0-2.72 1.344-5.024t3.616-3.648 5.024-1.344q3.616 0 6.368 2.272t3.424 5.728h-4.16q-0.608-1.76-2.144-2.88t-3.488-1.12q-2.496 0-4.256 1.76t-1.728 4.256v4h16q0.8 0 1.408 0.576t0.576 1.408v14.016q0 0.832-0.576 1.408t-1.408 0.576h-20q-0.832 0-1.44-0.576t-0.576-1.408zM8 28h16v-9.984h-16v9.984z";
inline juce::String lockSVG =
    "M4 30.016q0 0.832 0.576 1.408t1.44 0.576h20q0.8 0 1.408-0.576t0.576-1.408v-14.016q0-0.832-0.576-1.408t-1.408-0.576v-4q0-2.048-0.8-3.872t-2.144-3.2-3.2-2.144-3.872-0.8q-2.72 0-5.024 1.344t-3.616 3.648-1.344 5.024v4q-0.832 0-1.44 0.576t-0.576 1.408v14.016zM8 28v-9.984h16v9.984h-16zM10.016 14.016v-4q0-2.496 1.728-4.256t4.256-1.76 4.256 1.76 1.76 4.256v4h-12z";
inline juce::String chainSVG =
    "M0 18.016q0 2.496 1.76 4.224t4.256 1.76h1.984q1.952 0 3.488-1.12t2.144-2.88h-7.616q-0.832 0-1.44-0.576t-0.576-1.408v-4q0-0.832 0.576-1.408t1.44-0.608h7.616q-0.608-1.76-2.144-2.88t-3.488-1.12h-1.984q-2.496 0-4.256 1.76t-1.76 4.256v4zM8 16q0 0.832 0.576 1.44t1.44 0.576h12q0.8 0 1.408-0.576t0.576-1.44-0.576-1.408-1.408-0.576h-12q-0.832 0-1.44 0.576t-0.576 1.408zM18.368 20q0.64 1.792 2.176 2.912t3.456 1.088h2.016q2.464 0 4.224-1.76t1.76-4.224v-4q0-2.496-1.76-4.256t-4.224-1.76h-2.016q-1.92 0-3.456 1.12t-2.176 2.88h7.648q0.8 0 1.408 0.608t0.576 1.408v4q0 0.832-0.576 1.408t-1.408 0.576h-7.648z";

class CircleEnableButton : public gin::PluginButton
{
public:
    CircleEnableButton(gin::Parameter* p) : PluginButton(p)
    {
    }

    CircleEnableButton(gin::Parameter* p, juce::Colour onColor) : PluginButton(p), onColor(onColor)
    {
    }

private:
    void paintButton(juce::Graphics& g, bool over, bool down) override
    {
        auto c = isEnabled() ? onColor : juce::Colours::darkgrey.withAlpha(0.5f);
        if (over || down)
            c = c.withMultipliedAlpha(1.2f);

        g.setColour(c);

        auto r = getLocalBounds().toFloat().reduced(2.0f);
        //if enabled draw a circle, otherwise drawa hollow circle
        //find a centered square that fits within the bounds
        auto square = r.getIntersection(r.withSizeKeepingCentre(r.getHeight(), r.getHeight()));
        if (getToggleState())
        {
            g.drawEllipse(square, 2.0f);
            g.fillEllipse(square.reduced(3));
        }
        else
        {
            g.drawEllipse(square, 2.0f);
        }
    }

    void parentHierarchyChanged() override
    {
        PluginButton::parentHierarchyChanged();
        auto a = wantsAccessibleKeyboard(*this);
        setWantsKeyboardFocus(a);
    }

    juce::Colour onColor;
};


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
        pitchOffsetKnob = new TextSlider(resonatorParams.pitchInSemis, colour);
        pitchOffsetKnob->setReadoutDecimals(2);
        resonatorFrequencyKnob = new TextSlider(resonatorParams.resonatorFrequency, colour);
        resonatorFrequencyKnob->setReadoutDecimals(0);
        decayTimeKnob = new TextSlider(resonatorParams.decayTime, colour);
        decayTimeKnob->setReadoutDecimals(1);
        dispersionKnob = new TextSlider(resonatorParams.dispersion, colour);
        dispersionKnob->setReadoutDecimals(0);
        loopFilterCutoffKnob = new TextSlider(resonatorParams.loopFilterCutoff, colour);
        loopFilterCutoffKnob->setReadoutDecimals(0);
        loopFilterPitchKnob = new TextSlider(resonatorParams.loopFilterPitchInSemis, colour);
        loopFilterPitchKnob->setReadoutDecimals(2);
        loopFilterResonanceKnob = new TextSlider(resonatorParams.loopFilterResonance, colour);
        loopFilterResonanceKnob->setReadoutDecimals(2);
        loopFilterModeKnob = new TextSlider(resonatorParams.loopFilterMode, colour);
        loopFilterModeKnob->setReadoutDecimals(2);
        postFilterCutoffKnob = new TextSlider(resonatorParams.postFilterCutoff, colour);
        postFilterCutoffKnob->setReadoutDecimals(0);
        postFilterPitchKnob = new TextSlider(resonatorParams.postFilterPitchInSemis, colour);
        postFilterPitchKnob->setReadoutDecimals(2);
        postFilterResonanceKnob = new TextSlider(resonatorParams.postFilterResonance, colour);
        postFilterResonanceKnob->setReadoutDecimals(2);
        postFilterModeKnob = new TextSlider(resonatorParams.postFilterMode, colour);
        postFilterModeKnob->setReadoutDecimals(2);

        resonatorKeytrack = new CircleEnableButton(resonatorParams.resonatorKeytrack, colour);
        postFilterKeytrack = new CircleEnableButton(resonatorParams.postFilterKeytrack, colour);
        loopFilterKeytrack = new CircleEnableButton(resonatorParams.loopFilterKeytrack, colour);


        addAndMakeVisible(enableButton);
        addAndMakeVisible(gainKnob);
        addAndMakeVisible(pitchOffsetKnob);
        addAndMakeVisible(resonatorFrequencyKnob);
        addAndMakeVisible(decayTimeKnob);
        addAndMakeVisible(dispersionKnob);
        addAndMakeVisible(loopFilterCutoffKnob);
        addAndMakeVisible(loopFilterPitchKnob);
        addAndMakeVisible(loopFilterResonanceKnob);
        addAndMakeVisible(loopFilterModeKnob);
        addAndMakeVisible(postFilterCutoffKnob);
        addAndMakeVisible(postFilterPitchKnob);
        addAndMakeVisible(postFilterResonanceKnob);
        addAndMakeVisible(postFilterModeKnob);

        addAndMakeVisible(resonatorKeytrack);
        addAndMakeVisible(postFilterKeytrack);
        addAndMakeVisible(loopFilterKeytrack);

        resonatorFrequencyKnob->setVisible(false);
        loopFilterPitchKnob->setVisible(false);
        postFilterPitchKnob->setVisible(false);

        watchParam(enableButton->parameter);
        watchParam(resonatorKeytrack->parameter);
        watchParam(loopFilterKeytrack->parameter);
        watchParam(postFilterKeytrack->parameter);
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
        resonatorKeytrack->setBounds(bounds.removeFromTop(PARAMETER_HEIGHT).reduced(3));
        bounds.removeFromTop(SPACING_Y_SMALL);
        resonatorFrequencyKnob->setBounds(bounds.removeFromTop(PARAMETER_HEIGHT));
        pitchOffsetKnob->setBounds(resonatorFrequencyKnob->getBounds());
        bounds.removeFromTop(SPACING_Y_LARGE);
        decayTimeKnob->setBounds(bounds.removeFromTop(PARAMETER_HEIGHT));
        bounds.removeFromTop(SPACING_Y_LARGE);
        dispersionKnob->setBounds(bounds.removeFromTop(PARAMETER_HEIGHT));
        bounds.removeFromTop(SPACING_Y_LARGE);
        loopFilterKeytrack->setBounds(bounds.removeFromTop(PARAMETER_HEIGHT).reduced(3));
        bounds.removeFromTop(SPACING_Y_SMALL);
        loopFilterCutoffKnob->setBounds(bounds.removeFromTop(PARAMETER_HEIGHT));
        loopFilterPitchKnob->setBounds(loopFilterCutoffKnob->getBounds());
        bounds.removeFromTop(SPACING_Y_SMALL);
        loopFilterResonanceKnob->setBounds(bounds.removeFromTop(PARAMETER_HEIGHT));
        bounds.removeFromTop(SPACING_Y_SMALL);
        loopFilterModeKnob->setBounds(bounds.removeFromTop(PARAMETER_HEIGHT));
        bounds.removeFromTop(SPACING_Y_LARGE);
        postFilterKeytrack->setBounds(bounds.removeFromTop(PARAMETER_HEIGHT).reduced(3));
        bounds.removeFromTop(SPACING_Y_SMALL);
        postFilterCutoffKnob->setBounds(bounds.removeFromTop(PARAMETER_HEIGHT));
        postFilterPitchKnob->setBounds(postFilterCutoffKnob->getBounds());
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

        pitchOffsetKnob->setVisible(resonatorParams.resonatorKeytrack->isOn());
        resonatorFrequencyKnob->setVisible(!resonatorParams.resonatorKeytrack->isOn());

        loopFilterPitchKnob->setVisible(resonatorParams.loopFilterKeytrack->isOn());
        loopFilterCutoffKnob->setVisible(!resonatorParams.loopFilterKeytrack->isOn());

        postFilterPitchKnob->setVisible(resonatorParams.postFilterKeytrack->isOn());
        postFilterCutoffKnob->setVisible(!resonatorParams.postFilterKeytrack->isOn());
    }

    gin::SVGPluginButton* enableButton;
    gin::Knob* gainKnob;
    TextSlider* resonatorFrequencyKnob;
    TextSlider* pitchOffsetKnob;
    TextSlider* decayTimeKnob;
    TextSlider* dispersionKnob;
    TextSlider* loopFilterCutoffKnob;
    TextSlider* loopFilterPitchKnob;
    TextSlider* loopFilterResonanceKnob;
    TextSlider* loopFilterModeKnob;
    TextSlider* postFilterCutoffKnob;
    TextSlider* postFilterPitchKnob;
    TextSlider* postFilterResonanceKnob;
    TextSlider* postFilterModeKnob;

    CircleEnableButton* resonatorKeytrack;
    CircleEnableButton* loopFilterKeytrack;
    CircleEnableButton* postFilterKeytrack;

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
        this->pitchOffsetKnob = new gin::Knob(resonatorParams.pitchInSemis);
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
