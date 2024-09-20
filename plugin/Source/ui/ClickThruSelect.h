#ifndef CLICKTHRUSELECT_H
#define CLICKTHRUSELECT_H

#include <JuceHeader.h>

#include "ResonariumLookAndFeel.h"
/**
An select-like that allows for click-through selection.
Designed to have the same layout and aesthetic as TextSlider.h
*/
class ClickThruSelect : public gin::ParamComponent, private gin::Parameter::ParameterListener  {
public:
    explicit ClickThruSelect(gin::Parameter* parameter, juce::Colour colour = juce::Colour()) : gin::ParamComponent(parameter), colour(colour)
    {
        if(colour != juce::Colour())
        {
            label.setColour(juce::Label::textColourId, colour);
            customColor = true;
        }
        addAndMakeVisible(label);
        label.setText(parameter->getUserValueText(), juce::dontSendNotification);
        label.setInterceptsMouseClicks(false, false);
        parameter->addListener(this);

    }

    void resized() override
    {
        label.setBounds(this->getLocalBounds());
        label.setJustificationType(juce::Justification::centred);
        label.setFont(label.getFont().withHeight(fontSize));
    }

    void lookAndFeelChanged() override
    {
        if(!customColor) label.setColour(juce::Label::textColourId, getLookAndFeel().findColour(ResonariumLookAndFeel::accentColourId));
    }

    void mouseDown(const juce::MouseEvent& event) override
    {
        int newValue = (static_cast<int>(parameter->getUserValue()) + 1) % static_cast<int>(parameter->getUserRangeEnd() + 1);
        label.setText(parameter->getUserValueText(), juce::dontSendNotification);
        parameter->setUserValue(newValue);

    }

    void enablementChanged() override
    {
        if(this->isEnabled())
        {
            label.setColour(juce::Label::textColourId, colour);
        } else
        {
            label.setColour(juce::Label::textColourId, juce::Colours::darkgrey);
        }
    }

    void valueUpdated(gin::Parameter* param) override
    {
        label.setText(parameter->getUserValueText(), juce::dontSendNotification);
    }

    bool customColor = false;
    juce::Colour colour;
    juce::Label label;
    float fontSize = 21.0f;
};



#endif //CLICKTHRUSELECT_H
