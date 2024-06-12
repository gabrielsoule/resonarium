/**
* Collection of commonly used UI components
 */
#ifndef RESONARIUMCOMPONENTS_H
#define RESONARIUMCOMPONENTS_H

#include <JuceHeader.h>
#include "../Parameters.h"
#include "../defines.h"

class ResonatorComponent : public gin::MultiParamComponent
{
public:
    ResonatorComponent(ResonatorParams resonatorParams) : resonatorParams(resonatorParams)
    {
        this->enableButton = new gin::SVGPluginButton(resonatorParams.enabled, gin::Assets::power);
        this->gainKnob = new gin::Knob(resonatorParams.gain);
        this->pitchOffsetKnob = new gin::Knob(resonatorParams.harmonic);
        this->dispersionKnob = new gin::Knob(resonatorParams.dispersion);
        this->decayTimeKnob = new gin::Knob(resonatorParams.decayTime);
        this->decayFilterCutoffKnob = new gin::Knob(resonatorParams.decayFilterCutoff);

        addAndMakeVisible(enableButton);
        addAndMakeVisible(gainKnob);
        addAndMakeVisible(pitchOffsetKnob);
        addAndMakeVisible(decayTimeKnob);
        addAndMakeVisible(dispersionKnob);
        addAndMakeVisible(decayFilterCutoffKnob);

        watchParam(resonatorParams.enabled);

        setSize(KNOB_W_SMALL, 300);
    }

    void resized() override
    {
        auto bounds = getLocalBounds();
        bounds.removeFromTop(7); //just a little off the top, please
        enableButton->setBounds(bounds.removeFromTop(12));
        gainKnob->setBounds(bounds.removeFromTop(KNOB_W_SMALL));
        pitchOffsetKnob->setBounds(bounds.removeFromTop(KNOB_W_SMALL));
        decayTimeKnob->setBounds(bounds.removeFromTop(KNOB_W_SMALL));
        dispersionKnob->setBounds(bounds.removeFromTop(KNOB_W_SMALL));
        decayFilterCutoffKnob->setBounds(bounds.removeFromTop(KNOB_W_SMALL));
    }

    void paramChanged() override
    {
        MultiParamComponent::paramChanged();
        for (auto c : this->getChildren())
        {
            if (c != enableButton) c->setEnabled(resonatorParams.enabled->isOn());
        }

        if(resonatorParams.enabled->isOn())
        {
            borderColor = findColour(gin::PluginLookAndFeel::accentColourId);
        } else
        {
            borderColor = findColour(gin::PluginLookAndFeel::title1ColourId);
        }
    }

    void paint (juce::Graphics& g) override
    {
        g.setColour (borderColor); // Set the color of the background
        float cornerSize = 10.0f; // Set the corner size for the rounded rectangle

        g.drawRoundedRectangle (getLocalBounds().reduced(2.0f,2.0f).toFloat(), cornerSize, 2); // Draw the rounded rectangle
    }

    const float padding = 4.0f;
    juce::Colour borderColor;
    gin::SVGPluginButton* enableButton;
    gin::Knob* gainKnob;
    gin::Knob* pitchOffsetKnob;
    gin::Knob* decayTimeKnob;
    gin::Knob* dispersionKnob;
    gin::Knob* decayFilterCutoffKnob;

    ResonatorParams resonatorParams;


};


#endif //RESONARIUMCOMPONENTS_H
