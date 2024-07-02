/**
* Collection of commonly used UI components
 */
#ifndef RESONARIUMCOMPONENTS_H
#define RESONARIUMCOMPONENTS_H

#include <JuceHeader.h>
#include "../Parameters.h"
#include "../defines.h"

/**
 * Component containing a vertical stack of controls corresponding to a single waveguide resonator.
 */
class WaveguideResonatorComponent : public gin::MultiParamComponent
{
public:
    WaveguideResonatorComponent(ResonatorParams resonatorParams) : resonatorParams(resonatorParams)
    {
        this->enableButton = new gin::SVGPluginButton(resonatorParams.enabled, gin::Assets::power);
        this->gainKnob = new gin::Knob(resonatorParams.gain);
        this->pitchOffsetKnob = new gin::Knob(resonatorParams.harmonicInSemitones);
        this->dispersionKnob = new gin::Knob(resonatorParams.dispersion);
        this->decayTimeKnob = new gin::Knob(resonatorParams.decayTime);
        this->brightnessKnob = new gin::Knob(resonatorParams.eksFilterBrightness);
        // this->decayFilterCutoffKnob = new gin::Knob(resonatorParams.decayFilterCutoff);

        addAndMakeVisible(enableButton);
        addAndMakeVisible(gainKnob);
        addAndMakeVisible(pitchOffsetKnob);
        addAndMakeVisible(decayTimeKnob);
        addAndMakeVisible(dispersionKnob);
        addAndMakeVisible(brightnessKnob);
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
        brightnessKnob->setBounds(bounds.removeFromTop(KNOB_H_SMALL));
        // decayFilterCutoffKnob->setBounds(bounds.removeFromTop(KNOB_H_SMALL));
    }

    void paramChanged() override
    {
        // for some reason paramChanged() is being called before this component is added to the parent
        // this is fine, except that we can't access the lookandfeel at this point
        // this is a hack to get around that. There might be side effects. That's a later problem.
        if(this->getParentComponent() == nullptr)
        {
            return;
        }

        MultiParamComponent::paramChanged();
        for (auto c : this->getChildren())
        {
            if (c != enableButton) c->setEnabled(resonatorParams.enabled->isOn());
        }

        if(resonatorParams.enabled->isOn())
        {
            borderColor = findColour(gin::PluginLookAndFeel::accentColourId, true);
        } else
        {
            borderColor = findColour(gin::PluginLookAndFeel::title1ColourId, true);
        }
    }

    void paint (juce::Graphics& g) override
    {
        g.setColour (borderColor); // Set the color of the background
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
        if(this->getParentComponent() == nullptr)
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


#endif //RESONARIUMCOMPONENTS_H
