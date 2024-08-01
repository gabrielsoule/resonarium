#ifndef TEXTSLIDER_H
#define TEXTSLIDER_H


#include <JuceHeader.h>

#include "ResonariumLookAndFeel.h"
/**
* An hijacking, er, extension of gin::Knob that essentially creates a text field that can be interacted with as if it was a knob.
* The user can slide the mouse up and down to change the value of the text field.
* The knob still exists: Juce's rotary slider implementation is perfectly good, and we don't want to reinvent it.
* However, the knob itself is invisible, via a custom LookAndFeel. Clicks are passed through to the text field.
*
* It's a bit of a hack, but it'll do.
*/

class CustomizableReadout : public gin::Readout
{
public:
    CustomizableReadout(gin::Parameter* parameter) : gin::Readout(parameter), p(parameter)
    {
        this->getProperties().set("customFont", true);
    }

    void valueUpdated(gin::Parameter*) override
    {
        float value = p->getUserValue();
        setText(decimals > 0 ? juce::String(value, decimals) : juce::String(static_cast<int>(value)), juce::dontSendNotification);
        this->toBack();
    }

    void resized() override
    {
        gin::Readout::resized();
        this->setFont(this->getLookAndFeel().getLabelFont(*this).withHeight(fontSize));
    }

    // juce::TextEditor* createEditorComponent() override
    // {
    //     auto e = juce::Label::createEditorComponent();
    //     e->removeColour (juce::TextEditor::textColourId);
    //     e->setColour (juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
    //     e->setColour (juce::TextEditor::focusedOutlineColourId, juce::Colours::transparentBlack);
    //     e->applyFontToAllText (getLookAndFeel().getLabelFont (*this), true);
    //     e->setJustification (getJustificationType());
    //     return e;
    // }

    void showEditorAndMoveToFront()
    {
        this->showEditor();
        this->getCurrentTextEditor()->getProperties().set("customFont", true);
        this->getCurrentTextEditor()->toFront(true);
    }

    float fontSize = 21.0f;
    size_t decimals = 2;
    gin::Parameter* p;
};


class TextSlider : public gin::Knob {
public:

    TextSlider(gin::Parameter* parameter, bool fromCentre = false) : gin::Knob(parameter, fromCentre), mainReadout(parameter)
    {
        //instruct the LookAndFeel not to draw this knob; only works with the custom ResonariumLookAndFeel
        this->getSlider().getProperties().set("textOnly", true);
        addAndMakeVisible(mainReadout);
        this->knob.toFront(false);
        this->knob.setDoubleClickReturnValue(false, 0);
        setMouseCursor(juce::MouseCursor::UpDownResizeCursor);
    }

    TextSlider(gin::Parameter* parameter, juce::Colour textColour, bool fromCentre = false) : TextSlider(parameter, fromCentre)
    {

        mainReadout.setColour(juce::Label::textColourId, textColour);
        this->textColour = textColour;
    }

    void mouseEnter(const juce::MouseEvent& event) override
    {
        Knob::mouseEnter(event);
        setMouseCursor(juce::MouseCursor::UpDownResizeCursor);
    }

    //can't pass click events through the knob area (otherwise the knob wouldn't work)
    //so we force it here
    void mouseDoubleClick(const juce::MouseEvent& event) override
    {
        mainReadout.showEditorAndMoveToFront();
        getReadout().setVisible(false);
    }

    void paint(juce::Graphics& g) override
    {
        getReadout().setVisible(false);
        name.setVisible(false);
        gin::Knob::paint(g);
    }

    void enablementChanged() override
    {
        if(this->isEnabled())
        {
            mainReadout.setColour(juce::Label::textColourId, textColour);
        } else
        {
            mainReadout.setColour(juce::Label::textColourId, juce::Colours::darkgrey);
        }
    }

    void resized() override
    {
        gin::Knob::resized();
        this->knob.toFront(false);
        this->modDepthSlider.toFront(false);
        auto bounds = getLocalBounds();
        mainReadout.setBounds(this->getLocalBounds());
        mainReadout.setFont(mainReadout.getFont().withHeight(fontSize));
        mainReadout.setJustificationType(juce::Justification::centred);
        getReadout().setVisible(false);
        mainReadout.valueUpdated(this->parameter);
    }

    void setReadoutDecimals(size_t decimals)
    {
        mainReadout.decimals = decimals;
    }

    float fontSize = 21.0f;
    size_t decimals = 2.0f;
    CustomizableReadout mainReadout;
    juce::Colour textColour;
};



#endif //TEXTSLIDER_H
