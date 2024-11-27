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
        this->setFont(this->getLookAndFeel().getLabelFont(*this).withHeight(fontSize));
    }

    void valueUpdated(gin::Parameter*) override
    {
        float value = p->getUserValue();
        // DBG("value: " << value);
        if (paramToTextConversionFunction != nullptr)
        {
            value = paramToTextConversionFunction(value);
        }
        // DBG("converted value: " << value);
        juce::String text = juce::String(value, decimals);

        //remove erroneous negative zero if zero
        if(value == 0.0f && text.contains("-"))
        {
            //remove the negative sign
            text = text.substring(1);
        }
        setText(decimals > 0 ? text : juce::String(static_cast<int>(value)), juce::dontSendNotification);
        this->onTextChange();
        this->toBack();
    }

    void textWasEdited() override
    {
        if (applyValue (getText()))
        {
            DBG("text " << getText());
            float v = getText().getFloatValue();
            DBG("v: " << v);
            if(textToParamConversionFunction) v = textToParamConversionFunction(v);
            p->setUserValueAsUserAction (v);
        }
        repaint();
    }

    void resized() override
    {
        gin::Readout::resized();

    }

    void paint(juce::Graphics& g) override
    {
        if (! isBeingEdited())
            Label::paint (g);
        auto textBounds = getBorderSize().subtractedFrom(getLocalBounds()).toFloat();

        // Measure the width of the current text

        //compute the length of the label text, and add the unit "s"
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
    float unitFontSize = 12.0f;
    size_t decimals = 2;
    gin::Parameter* p;
    std::function<float(float)> textToParamConversionFunction = nullptr;
    std::function<float(float)> paramToTextConversionFunction = nullptr;
};


class TextSlider : public gin::Knob {
public:

    TextSlider(gin::Parameter* parameter, bool fromCentre = false) : gin::Knob(parameter, fromCentre), mainReadout(parameter)
    {
        //instruct the LookAndFeel not to draw this knob; only works with the custom ResonariumLookAndFeel
        getSlider().getProperties().set("textOnly", true);
        addAndMakeVisible(mainReadout);
        addAndMakeVisible(unitLabel);
        unitLabel.setVisible(false);
        knob.toFront(false);
        knob.setDoubleClickReturnValue(false, 0);
        unitLabel.setInterceptsMouseClicks(false, false);
        unitLabel.toFront(false);
        unitLabel.setEditable(false);
        //capitalize first letter or parameter.getLabel()
        juce::String unitText = parameter->getLabel().substring(0, 1).toUpperCase() + parameter->getLabel().substring(1);
        unitLabel.setText(unitText, juce::dontSendNotification);
        unitLabel.setFont(mainReadout.getFont().withHeight(11.0f));
        unitLabel.setJustificationType(juce::Justification::bottomLeft);
        setMouseCursor(juce::MouseCursor::UpDownResizeCursor);
        mainReadout.setMouseCursor(juce::MouseCursor::UpDownResizeCursor);
        knob.setMouseCursor(juce::MouseCursor::UpDownResizeCursor);
        mainReadout.onTextChange = [this] {
            mainReadoutTextWidth = mainReadout.getFont().getStringWidthFloat(mainReadout.getText());
            int unitWidth = 20;
            int unitHeight = 15;
            unitLabel.setBounds(juce::jmin(mainReadout.getBounds().getCentreX() + mainReadoutTextWidth / 2, static_cast<float>(mainReadout.getBounds().getRight())), getHeight() - unitHeight, unitWidth, unitHeight);
            unitLabel.setBounds(unitLabel.getBounds().translated(-3, -3));
        };
    }

    TextSlider(gin::Parameter* parameter, juce::Colour textColour, bool fromCentre = false) : TextSlider(parameter, fromCentre)
    {

        mainReadout.setColour(juce::Label::textColourId, textColour);
        this->textColour = textColour;
        getSlider().setColour(juce::Slider::thumbColourId, textColour);
    }

    ~TextSlider() override
    {
        gin::Knob::~Knob();
    }

    void mouseEnter(const juce::MouseEvent& event) override
    {
        Knob::mouseEnter(event);
        unitLabel.setVisible(true);
    }

    void mouseExit(const juce::MouseEvent& event) override
    {
        Knob::mouseExit(event);
        unitLabel.setVisible(false);
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
        // float textWidth = mainReadout.getFont().getStringWidthFloat(mainReadout.getText());
        //
        // // Set up the font for the unit
        // g.setFont(mainReadout.getFont().withHeight(8.0f));
        // g.setColour(mainReadout.findColour(juce::Label::textColourId));
        //
        // // Draw the unit
        // g.drawText(parameter->getLabel(), getLocalBounds().getCentreX() + textWidth / 2.0f - 2.0f, getLocalBounds().getBottom() - 2.0f, 30, 30,
        //            juce::Justification::bottomLeft, true);

    }

    void enablementChanged() override
    {
        if(this->isEnabled())
        {
            mainReadout.setColour(juce::Label::textColourId, textColour);
            unitLabel.setColour(juce::Label::textColourId, textColour);
        } else
        {
            mainReadout.setColour(juce::Label::textColourId, juce::Colours::darkgrey);
            unitLabel.setColour(juce::Label::textColourId, juce::Colours::darkgrey);
        }
    }

    void resized() override
    {
        gin::Knob::resized();
        this->knob.toFront(false);
        this->modDepthSlider.toFront(false);
        auto bounds = getLocalBounds();
        mainReadout.setBounds(this->getLocalBounds().reduced(10, 0));
        mainReadout.setFont(mainReadout.getFont().withHeight(fontSize));
        mainReadout.setJustificationType(juce::Justification::centred);
        getReadout().setVisible(false);
        mainReadout.valueUpdated(this->parameter);
        //set the unit label to be on the right edge of the main readout's text bounds
        mainReadout.onTextChange();
    }

    void setReadoutDecimals(size_t decimals)
    {
        mainReadout.decimals = decimals;
    }

    float fontSize = 23.0f;
    size_t decimals = 2.0f;
    CustomizableReadout mainReadout;
    float mainReadoutTextWidth;
    juce::Label unitLabel;
    juce::Colour textColour;
};



#endif //TEXTSLIDER_H
